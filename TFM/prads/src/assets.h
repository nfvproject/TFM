#include "SDMBN.h"
#define ASSET_HASH4(ip) ((ip) % BUCKET_SIZE)

#define ASSET_HASH6(ip) ( (ip).s6_addr32[3] % BUCKET_SIZE )

void add_asset(packetinfo *pi);
void del_asset(asset * passet, asset ** bucket_ptr);
void del_os_asset(os_asset ** prev_os, os_asset * passet);
void del_serv_asset(serv_asset ** prev_service, serv_asset * passet);
void update_asset(packetinfo *pi);
short update_asset_os(packetinfo *pi, uint8_t detection, bstring raw_fp, fp_entry *match, int uptime);
short update_asset_service(packetinfo *pi, bstring service, bstring application);
short update_asset_arp(u_int8_t arp_sha[MAC_ADDR_LEN], packetinfo *pi);
void clear_asset_list();
void update_asset_list();
void update_service_stats(int role, uint16_t proto);
uint8_t asset_lookup(packetinfo *pi);
asset* get_asset_per_sip(uint32_t sip);
void put_asset_per_sip(uint32_t sip, asset* in_asset, connection *cxt);
char* serialize_conn_asset(connection *conn, int msgid);
int find_del_asset(asset *masset);
int decr_asset_ref(asset *masset);
int incr_asset_ref(asset *masset);
int del_all_multiflows();
