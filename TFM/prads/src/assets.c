#include <sys/time.h>
#include <string.h>
#include "common.h"
#include "prads.h"
#include "assets.h"
#include "sys_func.h"
#include "output-plugins/log.h"
#include "config.h"
#include "mac.h"
#include <SDMBN.h>
#include "SDMBNLocal.h"

extern globalconfig config;
static asset *passet[BUCKET_SIZE];

long overall_mserz_time = 0;
long overall_mdeserz_time = 0;
long overall_mstate_size =0;

void update_asset(packetinfo *pi)
{
    
    if (asset_lookup(pi) == SUCCESS) {
        if (pi->asset != NULL) {
            pthread_mutex_lock(&AssetEntryLock);
            pi->asset->vlan = pi->vlan;
            pi->asset->last_seen = pi->pheader->ts.tv_sec;
            pthread_mutex_unlock(&AssetEntryLock);
        } else {
            printf("\nBAD ERROR in update_asset\n");
        }
    } else {
        add_asset(pi);
    }
    return;
}

void update_service_stats(int role, uint16_t proto)
{
    if (role==SC_SERVER) {
        if (proto== 6) config.pr_s.tcp_services++;
        if (proto==17) config.pr_s.udp_services++;
    } else {
        if (proto== 6) config.pr_s.tcp_clients++;
        if (proto==17) config.pr_s.udp_clients++;
    }
}

void update_os_stats(uint8_t detection)
{
    switch (detection) {
        // fallthrough
        case CO_SYN:
        case CO_SYNACK:
        case CO_ACK:
        case CO_FIN:
        case CO_RST:
            config.pr_s.tcp_os_assets++;
            break;
        case CO_UDP:
            config.pr_s.udp_os_assets++;
            break;
        case CO_ICMP:
            config.pr_s.icmp_os_assets++;
            break;
        case CO_DHCP:
            config.pr_s.dhcp_os_assets++;
            break;
        default:
            break;
    }
}

void flip_connection(packetinfo *pi, asset* masset)
{
   if (pi->sc == SC_CLIENT) {
      if (pi->cxt->reversed == 0)
         pi->cxt->c_asset = masset;
      else 
         pi->cxt->s_asset = masset;
   } else {
      if (pi->cxt->reversed == 0)
         pi->cxt->s_asset = masset;
      else
         pi->cxt->c_asset = masset;
   }
   if (pi->sc == SC_CLIENT) {
      if (pi->cxt->reversed == 0)
         pi->cxt->c_asset = masset;
      else
         pi->cxt->s_asset = masset;
   } else {
      if (pi->cxt->reversed == 0)
         pi->cxt->s_asset = masset;
      else
         pi->cxt->c_asset = masset;
   }
}

///// SDMBN Support Functions ///////////////////////////////////////////////
asset* get_asset_per_sip(uint32_t sip)
{
    uint64_t hash;
    asset *masset = NULL;
     
    hash = ASSET_HASH4(sip);
    pthread_mutex_lock(&AssetEntryLock);        
    masset = passet[hash];
    while (masset != NULL) {
        if (masset->af == AF_INET && CMP_ADDR4( &masset->ip_addr, sip))
        {
            INFO_PRINT("get asset_per_sp %d", masset->vlan);
            /*if (curr != NULL && state != NULL)
            {
                curr->c_asset = masset;
                *state = ser_ialize(head_tra, "connection", curr, NULL, 0);
            }*/
            pthread_mutex_unlock(&AssetEntryLock); 
            return masset;
        }
        masset = masset->next;
    }
    
    pthread_mutex_unlock(&AssetEntryLock); 
    return masset;
}

void update_osassets_per_asset(asset *l_asset, asset *in_asset, 
        connection *cxt)
{
    os_asset *tmp_oa = NULL;
    os_asset *head_oa = NULL;
    os_asset *in_oa = NULL;
    tmp_oa = l_asset->os;
    head_oa = l_asset->os;
    in_oa = in_asset->os;
    while (in_oa)
    {
        while (tmp_oa != NULL) 
        {
            if (in_oa->detection == tmp_oa->detection) 
            {
                if (in_oa->raw_fp) 
                {
                    // old style save-the-fp-string OS detection
                    if (biseq(in_oa->raw_fp, tmp_oa->raw_fp) == 1) 
                    {
                        /* Found! */
                        // FIXME: inefficient string copies
                        bdestroy(tmp_oa->raw_fp);
                        tmp_oa->raw_fp = bstrcpy(in_oa->raw_fp);
                        //tmp_sa->i_attempts++;
                        tmp_oa->port = in_oa->port;
                        tmp_oa->last_seen = in_oa->last_seen;
                        tmp_oa->uptime = in_oa->uptime;
                        break;
                    }
                }
		// [TODO]Fix this
                /*else if (in_oa->fp)
                {
                    if (in_oa->fp.os == tmp_oa->fp.os &&
                        in_oa->fp.desc == tmp_oa->fp.desc){
                        // pointer equality - does this OS asset point
                        // to the same match?
                        //if (match == tmp_oa->match) {
                        tmp_oa->port = in_oa->port;
                        tmp_oa->last_seen = in_oa->last_seen;
                        tmp_oa->uptime = in_oa->uptime;
                        break;
                }*/
            }
            tmp_oa = tmp_oa->next;
        }

        if (tmp_oa == NULL) 
        {
            //update_os_stats(detection);
            os_asset *new_oa = NULL;

            // FIXME: allocate resource from shared storage pool
            new_oa = (os_asset *) calloc(1, sizeof(os_asset));
            new_oa->detection = in_oa->detection;

            if (in_oa->raw_fp) 
            {
                // FIXME: don't copy fp, bincode it
                new_oa->raw_fp = bstrcpy(in_oa->raw_fp);
            } 
            /*else if(in_oa->fp) 
            {
                // copy the match
                new_oa->fp = in_oa->fp;
                //new_oa->fp = *match; //why copy it two times?
            }*/
            //new_oa->i_attempts = 1;
            new_oa->first_seen = in_oa->first_seen;
            new_oa->last_seen = in_oa->last_seen;
            new_oa->port = in_oa->port;
            new_oa->ttl = in_oa->ttl;
            new_oa->uptime = in_oa->uptime;
            new_oa->next = head_oa;
            if (head_oa != NULL)
            { head_oa->prev = new_oa; }
            new_oa->prev = NULL;
            l_asset->os = new_oa;

            if (cxt != NULL)
            { log_asset_os(l_asset,new_oa,cxt); }
        }
        in_oa = in_oa->next;
    }
}

void update_sassets_per_asset(asset *l_asset, asset *in_asset, 
        connection *cxt)
{
    serv_asset *tmp_sa = NULL;
    serv_asset *head_sa = NULL;
    serv_asset *in_sa = NULL;
    uint16_t port;

    in_sa = in_asset->services;
    while (in_sa)
    {
         /* Find asset within linked list */
         tmp_sa = head_sa = l_asset->services;
         port = in_sa->port;
 
         while (tmp_sa != NULL) 
         {
             if (in_sa->port == tmp_sa->port 
                     && in_sa->proto == tmp_sa->proto) 
             {
                 if (in_sa->last_seen < tmp_sa->last_seen)
                 { break; }
                
                 if ((in_sa->application->data[0] != '@') 
                         && (tmp_sa->application->data[0] == '@')) 
                 {
                     tmp_sa->i_attempts = 0;
                     bdestroy(tmp_sa->service);
                     bdestroy(tmp_sa->application);
                     tmp_sa->service = bstrcpy(in_sa->service);
                     tmp_sa->application = bstrcpy(in_sa->application);
                     tmp_sa->last_seen = in_sa->last_seen;

                     log_asset_service(l_asset,tmp_sa, cxt);
                     
                 } 
                 else if (!(biseq(in_sa->application, tmp_sa->application) 
                             == 1)) 
                 {
                     if (tmp_sa->i_attempts > MAX_SERVICE_CHECK) 
                     {
                         tmp_sa->i_attempts = 0;
                         bdestroy(tmp_sa->service);
                         bdestroy(tmp_sa->application);
                         tmp_sa->service = bstrcpy(in_sa->service);
                         tmp_sa->application = bstrcpy(in_sa->application);
                         tmp_sa->last_seen = in_sa->last_seen;
                         log_asset_service(l_asset,tmp_sa, cxt);
                     } 
                     else 
                     {
                        tmp_sa->i_attempts++;
                        tmp_sa->last_seen = in_sa->last_seen;
                     }
                } 
                else 
                {
                    tmp_sa->i_attempts = 0;
                    tmp_sa->last_seen = in_sa->last_seen;
                    
                }
                break;
            }
            tmp_sa = tmp_sa->next;
        }

        if (tmp_sa == NULL) 
        {
            // update_service_stats(pi->sc, pi->proto);
            serv_asset *new_sa = NULL;
            new_sa = (serv_asset *) calloc(1, sizeof(serv_asset));
            new_sa->port = in_sa->port;
            new_sa->ttl = in_sa->ttl;
            new_sa->proto = in_sa->proto;
            new_sa->service = bstrcpy(in_sa->service);
            new_sa->application = bstrcpy(in_sa->application);
            new_sa->role = in_sa->role;
            new_sa->i_attempts = in_sa->i_attempts;
            new_sa->first_seen = in_sa->first_seen;
            new_sa->last_seen = in_sa->last_seen;
            new_sa->next = l_asset->services;
            new_sa->prev = NULL;
            l_asset->services = new_sa;

            if (cxt != NULL)
            { log_asset_service(l_asset, new_sa, cxt); }
        }
        in_sa = in_sa->next; 
    }
}

void put_asset_per_sip(uint32_t sip, asset* in_asset, connection *cxt)
{
    asset *l_asset = NULL, *temp_asset = NULL;
    char *temp;

    if (in_asset == NULL)
    { return; }

    l_asset = get_asset_per_sip(sip);
    in_asset->next = NULL;
    in_asset->prev = NULL;
    INFO_PRINT("[put_asset_per_sip]: Putting asset in the list..for IP %d",
            sip); 
    if (l_asset == NULL)
    {
        // Asset struct not found
        // Push the struct into the list
        INFO_PRINT("[put_asset_per_sip]: Asset structure not found"); 
        uint64_t hash;
        pthread_mutex_lock(&AssetEntryLock);
        hash = ASSET_HASH4(sip);
        in_asset->next = passet[hash];

        if (passet[hash] != NULL)
        { passet[hash]->prev = in_asset; }
    
        passet[hash] = in_asset;
        in_asset->ref = 0;
        
        pthread_mutex_unlock(&AssetEntryLock); 
        return;
    }
    
    // Asset struct found update
    // Update the structs if required
    INFO_PRINT("[put_asset_per_sip]: Asset structure found");
    if (l_asset->last_seen > in_asset->last_seen)
    { return; }

    l_asset->vlan = in_asset->vlan;
    l_asset->last_seen = in_asset->last_seen;
     
    INFO_PRINT("[put_asset_per_sip]: Updating OS and Service Assets");
    update_sassets_per_asset(l_asset, in_asset, cxt);
    update_osassets_per_asset(l_asset, in_asset, cxt);
    // Delete the asset as the deltas have now been logged
    del_asset(in_asset, &temp_asset);
}
/////////////////////////////////////////////////////////////////////////////

asset* connection_lookup(packetinfo *pi)
{
   if(NULL == pi->cxt){
      return NULL;
   }

   if (pi->sc == SC_CLIENT && pi->cxt->reversed == 0 && pi->cxt->c_asset != NULL) {
      return pi->cxt->c_asset;
   } else if (pi->sc == SC_CLIENT && pi->cxt->reversed == 1 && pi->cxt->s_asset != NULL) {
      return pi->cxt->s_asset;
   } else if (pi->sc == SC_SERVER && pi->cxt->reversed == 0 && pi->cxt->s_asset != NULL) {
      return pi->cxt->s_asset;
   } else if (pi->sc == SC_SERVER && pi->cxt->reversed == 1 && pi->cxt->c_asset != NULL) {
      return pi->cxt->c_asset;
   }
   return NULL;
}

///// SDMBN Support Functions ///////////////////////////////////////////////
void update_cxt_asset(packetinfo *pi, asset *masset)
{
   if(NULL == pi->cxt)
   {
      INFO_PRINT("[update_cxt_asset] pi->cxt is Null");
      return;
   }
   INFO_PRINT("[update_cxt_asset] Updating cxt asset");
   if (pi->sc == SC_CLIENT && pi->cxt->reversed == 0 ) 
   { pi->cxt->c_asset = masset; }
   else if (pi->sc == SC_CLIENT && pi->cxt->reversed == 1 ) 
   { pi->cxt->s_asset = masset; }
   else if (pi->sc == SC_SERVER && pi->cxt->reversed == 0 )
   { pi->cxt->s_asset = masset; }
   else if (pi->sc == SC_SERVER && pi->cxt->reversed == 1 )
   { pi->cxt->c_asset = masset; }
   __sync_fetch_and_add(&masset->ref,1); // take a ref for cxt
}
/////////////////////////////////////////////////////////////////////////////

/* asset *asset_lookup(struct in6_addr ip, int af)
 * tries to match your packet to a sender we've seen before
 * 
 * 1. already in connection database
 * 2. ip4 lookup & ip6 lookup
 * 3. mac lookup
 *
 * asset_lookup should return 0 on success, 1 on failure
 */
uint8_t asset_lookup(packetinfo *pi)
{
    uint64_t hash;
    asset *masset = NULL;

    if (pi->asset != NULL || NULL != (pi->asset = connection_lookup(pi))){
       return SUCCESS;
    } else {
        if (pi->af == AF_INET) {
            uint32_t ip;

            if(pi->arph) {// arp check
                //memcpy(&ip, pi->arph->arp_spa, sizeof(uint8_t)*4);
                ip  = *(uint32_t*) pi->arph->arp_spa;
            } else {
                ip = PI_IP4SRC(pi);
            }
            hash = ASSET_HASH4(ip);
            pthread_mutex_lock(&AssetEntryLock); 
            masset = passet[hash];
            while (masset != NULL) {
                //if (memcmp(&ip_addr,&rec->ip_addr,16)) {
                if (masset->af == AF_INET 
                    && CMP_ADDR4( &masset->ip_addr, ip))
                {
                    pi->asset = masset;
                    update_cxt_asset(pi,masset);
                    pthread_mutex_unlock(&AssetEntryLock);
                    return SUCCESS;
                }
                masset = masset->next;
            }
            pthread_mutex_unlock(&AssetEntryLock); 
            return ERROR;
        } else if (pi->af == AF_INET6) {
            hash = ASSET_HASH6(PI_IP6SRC(pi));
            pthread_mutex_lock(&AssetEntryLock);
            masset = passet[hash];
            while (masset != NULL) {
                if (masset->af == AF_INET6 &&
                    CMP_ADDR6(&masset->ip_addr, &PI_IP6SRC(pi))){
                    pi->asset = masset;
                    pthread_mutex_unlock(&AssetEntryLock); 
                    return SUCCESS;
                }
                masset = masset->next;
            }
            pthread_mutex_unlock(&AssetEntryLock); 
            return ERROR;
        }
        return ERROR;
    }
}

/* ----------------------------------------------------------
 * FUNCTION     : update_asset_os
 * DESCRIPTION  : This function will update the OS
 *              : fields of an asset.
 * INPUT        : 0 - IP Address
 *              : 1 - Port
 *              : 2 - detection method
 *              : 3 - raw_fp
 *              : 4 - AF_INET/6
 *              : 5 - uptime
 * RETURN       : 0 - Success!
 *              : 1 - Failure!
 * ---------------------------------------------------------- */

short update_asset_shmem(packetinfo *pi)
{
    // flip it upside down: caller packs it?
    // now how would that eat the program from the inside?
    // pass the struct around but store it in a shared mem buffer
    (void)pi->s_port;
    // what is detection?
    //detection;
    // must include this
    //pi->raw_fp;
    (void)pi->af;
    //pi->uptime;
    // what more do we need in the *pi?
    switch(pi->type){
        case SIGNATURE:
        // signatures identify HOSTS or SERVICES
        // (or mac resources) .. eventually have graph method
            return 1; // we did the computation straight on the *pi;
        case FINGERPRINT:
            //update_asset_os_shmem(pi);
            return 2; // whatever
        default:
            return 1338; // not leet.
    }
}

short update_asset_os (
    packetinfo *pi,
    uint8_t detection,
    bstring raw_fp,
    fp_entry *match,
    int uptime
    )
{
    os_asset *tmp_oa = NULL;
    os_asset *head_oa = NULL;

    if (asset_lookup(pi) == SUCCESS) {
        if (pi->asset != NULL) {
            goto os_update;
        } else {
            elog("BAD ERROR in update_asset_os\n");
            return ERROR;
        }
    } else {
        update_asset(pi);
        if (update_asset_os(pi, detection, raw_fp, match, uptime) == SUCCESS) return SUCCESS;
            else return ERROR;
    }

os_update:
    tmp_oa = pi->asset->os;
    head_oa = pi->asset->os;
    pi->asset->last_seen = pi->pheader->ts.tv_sec;

    while (tmp_oa != NULL) {
        if (detection == tmp_oa->detection) {
            if (raw_fp) {
                // old style save-the-fp-string OS detection
                if (biseq(raw_fp, tmp_oa->raw_fp) == 1) {
                    /* Found! */
                    // FIXME: inefficient string copies
                    bdestroy(tmp_oa->raw_fp);
                    tmp_oa->raw_fp = bstrcpy(raw_fp);
                    //tmp_sa->i_attempts++;
                    if(pi->tcph)
                        tmp_oa->port = PI_TCP_SP(pi);
                    else
                        tmp_oa->port = 0;

                    tmp_oa->last_seen = pi->pheader->ts.tv_sec;
                    if (uptime) tmp_oa->uptime = uptime;
                    return SUCCESS;
                }
            }else if (match){
                // pointer equality - does this OS asset point
                // to the same match?
                /*if (match->os == tmp_oa->fp.os &&
                    match->desc == tmp_oa->fp.desc){
                //if (match == tmp_oa->match) {

                    if(pi->tcph)
                        tmp_oa->port = PI_TCP_SP(pi);
                    else
                        tmp_oa->port = 0;

                    tmp_oa->last_seen = pi->pheader->ts.tv_sec;
                    if (uptime)
                        tmp_oa->uptime = uptime;
                    return SUCCESS;
                }*/
            }
        }
        tmp_oa = tmp_oa->next;
    }

    if (tmp_oa == NULL) {
        update_os_stats(detection);
        os_asset *new_oa = NULL;

        // FIXME: allocate resource from shared storage pool
        new_oa = (os_asset *) calloc(1, sizeof(os_asset));
        new_oa->detection = detection;

        if (raw_fp) {
            // FIXME: don't copy fp, bincode it
            new_oa->raw_fp = bstrcpy(raw_fp);
        } else if(match) {
            // copy the match
            new_oa->fp = *match;
            //new_oa->fp = *match; //why copy it two times?
        }
        //new_oa->i_attempts = 1;
        new_oa->first_seen = pi->pheader->ts.tv_sec;
        new_oa->last_seen = pi->pheader->ts.tv_sec;

        if(pi->tcph)
            new_oa->port = PI_TCP_SP(pi);
        else
            new_oa->port = 0;

        if (pi->ip4 != NULL) new_oa->ttl = pi->ip4->ip_ttl;
            else if (pi->ip6 != NULL) new_oa->ttl = pi->ip6->hop_lmt;
        if (uptime) new_oa->uptime = uptime;
        new_oa->next = head_oa;
        if (head_oa != NULL)
            head_oa->prev = new_oa;
        new_oa->prev = NULL;
        pi->asset->os = new_oa;

        log_asset_os(pi->asset,new_oa, pi->cxt);
        return SUCCESS;
    }
    return ERROR;
}

/* ----------------------------------------------------------
 * FUNCTION     : update_asset_service
 * DESCRIPTION  : This function will update the service and
 *              : application fields of an asset.
 * INPUT        : 0 - IP Address
 *              : 1 - Port
 *              : 2 - Proto
 *              : 3 - Service
 *              : 4 - Application
 * RETURN       : 0 - Success!
 *              : 1 - Failure!
 * ---------------------------------------------------------- */
short update_asset_service(packetinfo *pi, bstring service, bstring application)
{
    serv_asset *tmp_sa = NULL;
    serv_asset *head_sa = NULL;
    uint16_t port;

    if (asset_lookup(pi) == SUCCESS) {
        if (pi->asset != NULL) {
            if (pi->sc == SC_CLIENT) {
                port = pi->d_port;
            } else {
                port = pi->s_port;
            }
            goto service_update;
        } else {
            printf("\nBAD ERROR in update_asset_service\n");
            return ERROR;
        }
    } else {
        /* If no asset */
        update_asset(pi);
        return update_asset_service(pi, service, application);
    }

service_update:

    /* Find asset within linked list */
    tmp_sa = head_sa = pi->asset->services;
    pi->asset->last_seen = pi->pheader->ts.tv_sec;    

    while (tmp_sa != NULL) {
        if (port == tmp_sa->port && pi->proto == tmp_sa->proto) {
            /*
             * Found! 
             * If we have an id for the service which is != unknown AND the id now is unknown 
             * - just increment i_attempts untill MAX_PKT_CHECK before replacing with unknown 
             *
             * NEW: No more unknown :)
             * But now we have generic service for the port, example: @https
             * So now we just check the first char of the string for '@'.
             * if (application->data[0] != '@') (If the service matched dont starts with a '@')
             *  and the service registered in the service_asset starts with '@', discard it and
             *  register the new asset!
             */
            if ((application->data[0] != '@') && (tmp_sa->application->data[0] == '@')) {
                tmp_sa->i_attempts = 0;
                bdestroy(tmp_sa->service);
                bdestroy(tmp_sa->application);
                tmp_sa->service = bstrcpy(service);
                tmp_sa->application = bstrcpy(application);
                tmp_sa->last_seen = pi->pheader->ts.tv_sec;

                log_asset_service(pi->asset,tmp_sa, pi->cxt);
                return SUCCESS;

            } else if (!(biseq(application, tmp_sa->application) == 1)) {
                if (tmp_sa->i_attempts > MAX_SERVICE_CHECK) {
                    tmp_sa->i_attempts = 0;
                    bdestroy(tmp_sa->service);
                    bdestroy(tmp_sa->application);
                    tmp_sa->service = bstrcpy(service);
                    tmp_sa->application = bstrcpy(application);
                    tmp_sa->last_seen = pi->pheader->ts.tv_sec;

                    log_asset_service(pi->asset,tmp_sa, pi->cxt);
                    return SUCCESS;

                } else {
                    tmp_sa->i_attempts++;
                    tmp_sa->last_seen = pi->pheader->ts.tv_sec;
                    return SUCCESS;
                }
            } else {
                tmp_sa->i_attempts = 0;
                tmp_sa->last_seen = pi->pheader->ts.tv_sec;
                return SUCCESS;
            }
        }
        tmp_sa = tmp_sa->next;
    }

    if (tmp_sa == NULL) {
        update_service_stats(pi->sc, pi->proto);
        serv_asset *new_sa = NULL;
        new_sa = (serv_asset *) calloc(1, sizeof(serv_asset));
        new_sa->port = port;
        if (pi->ip4 != NULL) new_sa->ttl = pi->ip4->ip_ttl;
            else if (pi->ip6 != NULL) new_sa->ttl = pi->ip6->hop_lmt;
        new_sa->proto = pi->proto;
        new_sa->service = bstrcpy(service);
        new_sa->application = bstrcpy(application);
        new_sa->role = pi->sc;
        new_sa->i_attempts = 0;
        new_sa->first_seen = pi->pheader->ts.tv_sec;
        new_sa->last_seen = pi->pheader->ts.tv_sec;
        new_sa->next = pi->asset->services;
        new_sa->prev = NULL;
        pi->asset->services = new_sa;

        log_asset_service(pi->asset, new_sa, pi->cxt);
        return SUCCESS;
    }
    return ERROR;
}

/* ----------------------------------------------------------
 * FUNCTION     : add_asset
 * DESCRIPTION  : This function will add an asset to the
 *              : specified asset data structure.
 * INPUT        : 0 - packetinfo (af, ip_src, vlan)
 * RETURN       : None!
 * ---------------------------------------------------------- */
void add_asset(packetinfo *pi)
{
    uint64_t hash;
    asset *masset = NULL;

    config.pr_s.assets++;

    masset = (asset *) calloc(1, sizeof(asset));
    masset->af = pi->af;
    masset->vlan = pi->vlan;
    masset->i_attempts = 0;
    masset->first_seen = masset->last_seen = pi->pheader->ts.tv_sec;

    if (pi->af == AF_INET) {
        if(pi->arph) // mongo arp check
            //memcpy(&masset->ip_addr.__u6_addr.__u6_addr32[0], pi->arph->arp_spa, sizeof(uint32_t));
           IP4ADDR(&masset->ip_addr) = *(uint32_t*) pi->arph->arp_spa;
        else
           IP4ADDR(&masset->ip_addr)  = PI_IP4SRC(pi);
        hash = ASSET_HASH4(IP4ADDR(&masset->ip_addr));
    } else if (pi->af == AF_INET6) {
        masset->ip_addr = PI_IP6SRC(pi);
        hash = ASSET_HASH6(PI_IP6SRC(pi));
    }

    pthread_mutex_lock(&AssetEntryLock); 
    masset->next = passet[hash];

    if (passet[hash] != NULL)
        passet[hash]->prev = masset;
    masset->prev = NULL;
    masset->os = NULL;
    masset->services = NULL;
    masset->macentry = NULL;
    passet[hash] = masset;
    pthread_mutex_unlock(&AssetEntryLock); 
    update_cxt_asset(pi, masset);

#ifdef DEBUGG
    /* verbose info for sanity checking */
    static char ip_addr_s[INET6_ADDRSTRLEN];
    // pi->ip_src does not exist!
    u_ntop(pi->ip_src, pi->af, ip_addr_s);
    dlog("[*] asset added: %s\n",ip_addr_s);
#endif
    
    //return masset;
}

short update_asset_arp(u_int8_t arp_sha[MAC_ADDR_LEN], packetinfo *pi)
{
    if (asset_lookup(pi) == SUCCESS) {
        if (pi->asset != NULL) {
            goto arp_update;
        } else {
            printf("\nBAD ERROR in update_asset_arp\n");
            return ERROR;
        }
    } else {

        add_asset(pi);
        update_asset(pi);

        if ( pi->asset == NULL ) {
            elog("update_asset(pi) failed! Asset does not exist! Cant enter MAC!!! die();\n");
            return ERROR;
        }
        if (update_asset_arp(arp_sha, pi) == SUCCESS) {
            return SUCCESS;
        } else {
            return ERROR;
        }
    }

arp_update:
    /* Check the ARP data structure for an existing entry */
    if (memcmp(pi->asset->mac_addr, arp_sha, MAC_ADDR_LEN) == 0) {
        /* UPDATE TIME STAMP */
        pi->asset->last_seen = pi->pheader->ts.tv_sec;
        return SUCCESS;
    } else {
        /* UPDATE MAC AND TIME STAMP */
        /* XXX: this handler suxx! */
        if(memcmp(pi->asset->mac_addr, "\0\0\0\0\0\0", 6)){
            printf("ACHTUNG! MAC changed! : ");
            print_mac(pi->asset->mac_addr);
            printf(" -> ");

            print_mac(arp_sha);
            printf("\n");

        }
        if(pi->asset->macentry == NULL) {
           // vendor entry did not exist.
           mac_entry *match = match_mac(config.sig_mac, arp_sha, 48);
           pi->asset->macentry = match;
        }
        memcpy(&pi->asset->mac_addr, arp_sha, MAC_ADDR_LEN);
        pi->asset->last_seen = pi->pheader->ts.tv_sec;
        log_asset_arp(pi->asset);
        return SUCCESS;
    }
}

void del_os_asset(os_asset ** head_oa, os_asset * os)
{

    if (os == NULL)
        return;
    os_asset *tmp_oa = NULL;
    os_asset *next_oa = NULL;
    os_asset *prev_oa = NULL;

    tmp_oa = os;
    //bdestroy(tmp_oa->vendor);
    //bdestroy(tmp_oa->os);
    bdestroy(tmp_oa->raw_fp);
    //bdestroy(tmp_oa->matched_fp);

    next_oa = tmp_oa->next;
    prev_oa = tmp_oa->prev;

    if (prev_oa == NULL) {
        /*
         * beginning of list 
         */
        *head_oa = next_oa;
        /*
         * not only entry 
         */
        if (next_oa)
            next_oa->prev = NULL;
    } else if (next_oa == NULL) {
        /*
         * at end of list! 
         */
        prev_oa->next = NULL;
    } else {
        /*
         * a node 
         */
        prev_oa->next = next_oa;
        next_oa->prev = prev_oa;
    }

    free(tmp_oa);
    tmp_oa = NULL;
    os = next_oa;
    return;

}

void del_serv_asset(serv_asset ** head_sa, serv_asset * service)
{

    if (service == NULL)
        return;
    serv_asset *tmp_sa = NULL;
    serv_asset *next_sa = NULL;
    serv_asset *prev_sa = NULL;

    tmp_sa = service;
    bdestroy(tmp_sa->service);
    bdestroy(tmp_sa->application);

    next_sa = tmp_sa->next;
    prev_sa = tmp_sa->prev;

    if (prev_sa == NULL) {
        /*
         * beginning of list 
         */
        *head_sa = next_sa;
        /*
         * not only entry 
         */
        if (next_sa)
            next_sa->prev = NULL;
    } else if (next_sa == NULL) {
        /*
         * at end of list! 
         */
        prev_sa->next = NULL;
    } else {
        /*
         * a node 
         */
        prev_sa->next = next_sa;
        next_sa->prev = prev_sa;
    }

    free(service);
    service = NULL;
    service = next_sa;
    return;
}

void del_asset(asset * passet, asset ** bucket_ptr)
{
    /*
     * remove passet from bucket 
     */
    asset *prev = passet->prev; /* OLDER connections */
    asset *next = passet->next; /* NEWER connections */
    serv_asset *tmp_sa = passet->services;
    os_asset *tmp_oa = passet->os;
    serv_asset *stmp = tmp_sa;
    os_asset *otmp = tmp_oa;

    /*
     * delete all service assets 
     */
    while (tmp_sa != NULL) {
        stmp = tmp_sa;
        tmp_sa = tmp_sa->next;
        del_serv_asset(&passet->services, stmp);
    }
    /*
     * delete all os assets 
     */
    while (tmp_oa != NULL) {
        otmp = tmp_oa;
        tmp_oa = tmp_oa->next;
        del_os_asset(&passet->os, otmp);
    }

    /*
     * now delete the asset 
     */
    if (prev == NULL) {
        // beginning of list
        *bucket_ptr = next;
        // not only entry
        if (next)
            next->prev = NULL;
    } else if (next == NULL) {
        // at end of list!
        prev->next = NULL;
    } else {
        // a node.
        prev->next = next;
        next->prev = prev;
    }

    /*
     * Free and set to NULL 
     */
    free(passet);
    passet = NULL;
}

void clear_asset_list()
{
    asset *rec = NULL;
    int akey;

    pthread_mutex_lock(&AssetEntryLock); 
    for (akey = 0; akey < BUCKET_SIZE; akey++) {
        rec = passet[akey];
        while (rec != NULL) {
            serv_asset *tmp_sa = NULL;
            os_asset   *tmp_oa = NULL;
            tmp_sa = rec->services;
            tmp_oa = rec->os;

            while (tmp_sa != NULL) {
                /* Delete service asset */
                serv_asset *stmp = tmp_sa;
                tmp_sa = tmp_sa->next;
                del_serv_asset(&rec->services, stmp);
            }

            while (tmp_oa != NULL) {
                /* Delete os asset */
                os_asset *otmp = tmp_oa;
                tmp_oa = tmp_oa->next;
                del_os_asset(&rec->os, otmp);
            }
                
            /* Delete the main asset */
            asset *tmp = rec;
            rec = rec->next;
            del_asset(tmp, &passet[akey]);
        }
    }
    pthread_mutex_unlock(&AssetEntryLock); 
    dlog("asset memory has been cleared\n");
}

/* update_asset_list()
 ** iterates over all assets,
 **** all services
 **** all OS matches
 ** and expires old (service, os, asset) since ASSET_TIMEOUT
 ** optionally printing assets updated since last SIG_ALRM check
 * */
void update_asset_list()
{
    extern time_t tstamp;
    asset *rec = NULL;
    int akey;
    serv_asset *tmp_sa = NULL;
    os_asset *tmp_oa = NULL;

    pthread_mutex_lock(&AssetEntryLock); 
    for (akey = 0; akey < BUCKET_SIZE; akey++) {
        rec = passet[akey];
        while (rec != NULL) {
            /* Checks if something has been updated in the asset since last time */
            if (tstamp - rec->last_seen <= SIG_ALRM) {
                tmp_sa = rec->services;
                tmp_oa = rec->os;
                if (config.print_updates) log_asset_arp(rec);

                while (tmp_sa != NULL) {
                    /* Just print out the asset if it is updated since lasttime */
                    if (config.print_updates && tstamp - tmp_sa->last_seen <= SIG_ALRM) {
                        log_asset_service(rec,tmp_sa, NULL);
                    }
                    /* If the asset is getting too old - delete it */
                    if (tstamp - tmp_sa->last_seen >= ASSET_TIMEOUT) {
                        serv_asset *stmp = tmp_sa;
                        tmp_sa = tmp_sa->next;
                        del_serv_asset(&rec->services, stmp);
                    } else {
                        tmp_sa = tmp_sa->next;
                    }
                }

                while (tmp_oa != NULL) {
                    /* Just print out the asset if it is updated since lasttime */
                    if (config.print_updates && tstamp - tmp_oa->last_seen <= SIG_ALRM) {
                        log_asset_os(rec, tmp_oa, NULL);
                    }
                    /* If the asset is getting too old - delete it */
                    if (tstamp - tmp_oa->last_seen >= ASSET_TIMEOUT) {
                        os_asset *otmp = tmp_oa;
                        tmp_oa = tmp_oa->next;
                        del_os_asset(&rec->os, otmp);
                    } else {
                        tmp_oa = tmp_oa->next;
                    }
                }
            }

            /* If nothing in the asset has been updated for some time - delete it! */
            if (tstamp - rec->last_seen >= ASSET_TIMEOUT) {
                asset *tmp = rec;
                rec = rec->next;
                del_asset(tmp, &passet[akey]);
            } else {
                rec = rec->next;
            }
        }
    }
    pthread_mutex_unlock(&AssetEntryLock); 
}

///// SDMBN Local Multiflow State Handlers ///////////////////////////////////
int local_get_multiflow(PerflowKey *key, int id)
{
    int count = 0;
    // Return multiflow state for a specific key
    if (key != NULL)
    {
        uint64_t hash;
        char *state;
        //asset* ast = NULL;
        asset *masset = NULL;
        
	// Check nw_src -- must be present to get specific key
        if (key->wildcards & WILDCARD_NW_SRC)
        { goto all; }
     
        //INFO_PRINT("Getting the asset for IP %d", key->nw_src);
        hash = ASSET_HASH4(key->nw_src);

        pthread_mutex_lock(&AssetEntryLock); 
        masset = passet[hash];
        while (masset != NULL) 
        {
        	if (masset->af == AF_INET 
       		    && CMP_ADDR4( &masset->ip_addr, key->nw_src))
        	{
	            INFO_PRINT("Found %d", masset->vlan);
       		    break;
        	}
        	masset = masset->next;
        }
        pthread_mutex_unlock(&AssetEntryLock); 	
        // No Asset found with the input key
        if (masset == NULL)
        { return 0; }
        
        struct timeval start_serialize, end_serialize;
        gettimeofday(&start_serialize, NULL);

        state = ser_ialize(head_tra, "asset", masset, NULL, 0);
        //char *state = ser_ialize(head_tra, "asset", ast, NULL, 0);
        //pthread_rwlock_unlock(&rwlock);
        gettimeofday(&end_serialize, NULL);
		long sec = end_serialize.tv_sec - start_serialize.tv_sec;
		long usec = end_serialize.tv_usec - start_serialize.tv_usec;
		long total = (sec * 1000 * 1000) + usec;
		overall_mserz_time += total;
		overall_mstate_size += strlen(state);
		printf("STATS: MULTIFLOW STATE SIZE CURRENT = %zu\n", strlen(state));
		printf("STATS: MULTIFLOW STATE SIZE OVERALL = %zu\n", overall_mstate_size);
		printf("STATS: MULTIFLOW: TIME TO SERIALIZE CURRENT = %ldus\n", total);
		printf("STATS: MULTIFLOW: TIME TO SERIALIZE OVERALL = %ldus\n", overall_mserz_time);

        SERIALIZE_PRINT("serializing multi flow state\n%s", state);

        // Construct key
        PerflowKey astkey;
        astkey.wildcards = WILDCARD_ALL;
        if (AF_INET == masset->af)
        {
            astkey.dl_type = 0x0800;
            astkey.nw_src = masset->ip_addr.s6_addr32[0];
            astkey.wildcards &= ~(WILDCARD_NW_SRC | WILDCARD_DL_TYPE);
        }
        else
        { /* FIXME: Handle IPv6 */ }
        int hashkey = astkey.nw_src;

        // Send multiflow state
        int result = sdmbn_send_multiflow(id, &astkey, state, hashkey, count);
        if (result < 0)
        { }

        // Increment count
        count++;

        // Clean-up
        free(state);
        return count;
    }
	else
		return -1;
    //INFO_PRINT("multiflow key is NULL");
all:
	printf("SRC_IP is wildcarded, getting all multiflow states");
    int h = 0;
    for (h = 0; h < BUCKET_SIZE; h++)
    {
        pthread_mutex_lock(&AssetEntryLock); 
        asset *curr = passet[h];
        while (curr != NULL)
        {

        	 struct timeval start_serialize, end_serialize;
        	 gettimeofday(&start_serialize, NULL);


            //pthread_rwlock_wrlock(&rwlock);
            char *state = ser_ialize(head_tra, "asset", curr, NULL, 0);
            //pthread_rwlock_unlock(&rwlock);

            gettimeofday(&end_serialize, NULL);
			long sec = end_serialize.tv_sec - start_serialize.tv_sec;
			long usec = end_serialize.tv_usec - start_serialize.tv_usec;
			long total = (sec * 1000 * 1000) + usec;
			overall_mserz_time += total;
			overall_mstate_size += strlen(state);
			printf("STATS: MULTIFLOW STATE SIZE CURRENT = %zu\n", strlen(state));
			printf("STATS: MULTIFLOW STATE SIZE OVERALL = %zu\n", overall_mstate_size);
			printf("STATS: MULTIFLOW: TIME TO SERIALIZE CURRENT = %ldus\n", total);
			printf("STATS: MULTIFLOW: TIME TO SERIALIZE OVERALL = %ldus\n", overall_mserz_time);

            // Construct key
            PerflowKey astkey;
            astkey.wildcards = WILDCARD_ALL;
            if (AF_INET == curr->af)
            {
                astkey.dl_type = 0x0800;
                astkey.nw_src = curr->ip_addr.s6_addr32[0];
                astkey.wildcards &= ~(WILDCARD_NW_SRC | WILDCARD_DL_TYPE);
            }
            else
            { /* FIXME: Handle IPv6 */ }
            int hashkey = astkey.nw_src;

            // Send multiflow state
            int result = sdmbn_send_multiflow(id, &astkey, state, hashkey, 
                    count);
            if (result < 0)
            { }

            // Clean-up
            free(state);

            // Increment count
            count++;

            // Move on to next asset
            curr = curr->next;
        }
        pthread_mutex_unlock(&AssetEntryLock); 
    }

    return count;
}

int local_put_multiflow(int hashkey, PerflowKey *key, char *state)
{
    if (NULL == key || NULL == state)
    { return -1; }

    SERIALIZE_PRINT("deserializing multi flow state\n%s", state);

    struct timeval start_deserialize, end_deserialize;
    gettimeofday(&start_deserialize, NULL);

    asset *ast = ser_parse(head_tra, "asset", state, NULL);
    gettimeofday(&end_deserialize, NULL);
	long sec = end_deserialize.tv_sec - start_deserialize.tv_sec;
	long usec = end_deserialize.tv_usec - start_deserialize.tv_usec;
	long total = (sec * 1000 * 1000) + usec;
	overall_mdeserz_time += total;
	printf("STATS: MULTIFLOW: TIME TO DESERIALIZE CURRENT = %ldus\n", total);
	printf("STATS: MULTIFLOW: TIME TO DESERIALIZE OVERALL = %ldus\n", overall_mdeserz_time);

    put_asset_per_sip(key->nw_src, ast, NULL);     

    return 1;
}

int del_all_multiflows()
{
    //INFO_PRINT("Clear Asset");
    int count = 0;
    asset *rec = NULL;
    int akey;
    pthread_mutex_lock(&AssetEntryLock);
    for (akey = 0; akey < BUCKET_SIZE; akey++) 
    {
        rec = passet[akey];
        while (rec != NULL) 
        {
            //if (!force && rec->ref > 0)
            //    continue;
            serv_asset *tmp_sa = NULL;
            os_asset   *tmp_oa = NULL;
            tmp_sa = rec->services;
            tmp_oa = rec->os;

            while (tmp_sa != NULL) 
            {
                // Delete service asset
                serv_asset *stmp = tmp_sa;
                tmp_sa = tmp_sa->next;
                del_serv_asset(&rec->services, stmp);
            }

            while (tmp_oa != NULL) 
            {
                // Delete os asset
                os_asset *otmp = tmp_oa;
                tmp_oa = tmp_oa->next;
                del_os_asset(&rec->os, otmp);
            }

            // Delete the main asset
            asset *tmp = rec;
            rec = rec->next;
            del_asset(tmp, &passet[akey]);
            count++;
        }
    }
    pthread_mutex_unlock(&AssetEntryLock);
    dlog("asset memory has been cleared\n");
    return count;
}

int find_del_asset(asset *masset)
{
    uint32_t sip;
    uint64_t hash;
    if (masset == NULL)
    { return 0; }
    sip = IP4ADDR(&masset->ip_addr);
    if (sip == 0)
    { return 0; }
    if (masset->ref > 0)
    { return 0; }

    hash = ASSET_HASH4(sip);
    // Clean-up
    pthread_mutex_lock(&AssetEntryLock);  
    del_asset(masset,&passet[hash]);
    pthread_mutex_unlock(&AssetEntryLock);
    return 1;
}

char* serialize_conn_asset(connection *conn, int msgid)
{ 
    asset *c_asset = NULL, *s_asset = NULL;

    c_asset = conn->c_asset;
    s_asset = conn->s_asset;
    pthread_mutex_lock(&AssetEntryLock);  
    if (c_asset != NULL)
    {
    	if (c_asset->sdmbn_msgid == msgid)
        {
            // temp set the conn->c_asset to NULL
            // to avoid retransmitting the same asset struct
            conn->c_asset = NULL; 
        }
        c_asset->sdmbn_msgid = msgid;
    }

    if (s_asset != NULL)
    {
        if (s_asset->sdmbn_msgid == msgid)
        {
            // temp set the conn->s_asset to NULL
            // to avoid retransmitting the same asset struct
            conn->s_asset = NULL;
        }
        s_asset->sdmbn_msgid = msgid;
    }

    char *state = ser_ialize(head_tra, "connection", conn, NULL, 0);
    // restore the asset pointers
    conn->c_asset = c_asset;
    conn->s_asset = s_asset;
    pthread_mutex_unlock(&AssetEntryLock); 
    return state;
}

int decr_asset_ref(asset *masset)
{ return __sync_sub_and_fetch(&masset->ref,1); }

int incr_asset_ref(asset *masset)
{ return __sync_add_and_fetch(&masset->ref,1); }
