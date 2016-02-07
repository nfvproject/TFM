package edu.wisc.cs.wisdom.sdmbn.operation;

import java.util.Arrays;
import java.util.concurrent.ConcurrentLinkedQueue;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.concurrent.Future;


import net.floodlightcontroller.core.IOFSwitch;
import java.io.InputStream;
import java.io.PrintStream;
import org.apache.commons.net.telnet.TelnetClient;
import java.io.IOException;

import edu.wisc.cs.wisdom.sdmbn.Middlebox;
import edu.wisc.cs.wisdom.sdmbn.PerflowKey;
import edu.wisc.cs.wisdom.sdmbn.Parameters.Guarantee;
import edu.wisc.cs.wisdom.sdmbn.Parameters.Optimization;
import edu.wisc.cs.wisdom.sdmbn.Parameters.Scope;
import edu.wisc.cs.wisdom.sdmbn.core.ReprocessEvent;
import edu.wisc.cs.wisdom.sdmbn.core.StateChunk;
import edu.wisc.cs.wisdom.sdmbn.json.DisableEventsMessage;
import edu.wisc.cs.wisdom.sdmbn.json.EnableEventsMessage;
import edu.wisc.cs.wisdom.sdmbn.json.EventsAckMessage;
import edu.wisc.cs.wisdom.sdmbn.json.GetMultiflowAckMessage;
import edu.wisc.cs.wisdom.sdmbn.json.GetMultiflowMessage;
import edu.wisc.cs.wisdom.sdmbn.json.GetPerflowAckMessage;
import edu.wisc.cs.wisdom.sdmbn.json.MigrateFinishAckMessage;
import edu.wisc.cs.wisdom.sdmbn.json.GetPerflowMessage;
import edu.wisc.cs.wisdom.sdmbn.json.Message;
import edu.wisc.cs.wisdom.sdmbn.json.MessageException;
import edu.wisc.cs.wisdom.sdmbn.json.PutAckMessage;
import edu.wisc.cs.wisdom.sdmbn.json.PutMultiflowAckMessage;
import edu.wisc.cs.wisdom.sdmbn.json.PutPerflowAckMessage;
import edu.wisc.cs.wisdom.sdmbn.json.ReprocessMessage;
import edu.wisc.cs.wisdom.sdmbn.json.StateMessage;
import edu.wisc.cs.wisdom.sdmbn.json.StateMultiflowMessage;
import edu.wisc.cs.wisdom.sdmbn.json.StatePerflowMessage;
import edu.wisc.cs.wisdom.sdmbn.utils.PacketUtil;
//wangynag
import java.io.InputStream;
import java.io.PrintStream;
import org.apache.commons.net.telnet.TelnetClient;

class NetTelnet {
	private TelnetClient telnet = new TelnetClient();
	private InputStream in;
	private PrintStream out;
	private char prompt = '$';

	// 普通用户结束
	public NetTelnet(String ip, int port) {
		try {
			telnet.connect(ip, port);
			in = telnet.getInputStream();
			out = new PrintStream(telnet.getOutputStream());
		} catch (Exception e) {
			e.printStackTrace();
		}
	}

	/** * 登录 * * @param user * @param password */
	public void login(String user, String password) {
		readUntil("login:");
		write(user);
		readUntil("Password:");
		write(password);
		readUntil(prompt + " ");
	}

	public String readUntil(String pattern) {
		try {
			char lastChar = pattern.charAt(pattern.length() - 1);
			StringBuffer sb = new StringBuffer();
			char ch = (char) in.read();
			while (true) {
				sb.append(ch);
				if (ch == lastChar) {
				    return sb.toString();
				}
				ch = (char) in.read();
			}
		} catch (Exception e) {
			e.printStackTrace();
		}
		return null;
	}

	public void write(String value) {
		try {
			out.println(value);
			out.flush();
		} catch (Exception e) {
			e.printStackTrace();
		}
	}

	/** * 向目标发送命令字符串 * * @param command * @return */
	public String sendCommand(String command) {
		try {
			write(command);
			return readUntil(prompt + " ");
		} catch (Exception e) {
			e.printStackTrace();
		}
		return null;
	}
	public void commitCommand(String command) {
		try {
			write(command);
		} catch (Exception e) {
			e.printStackTrace();
		}
		return;
	}

	/** * 关闭连接 */
	public void disconnect() {
		try {
			telnet.disconnect();
		} catch (Exception e) {
			e.printStackTrace();
		}
	}

}

public class MoveOperation extends Operation 
{
	// Arguments provided in constructor
	private Middlebox src;
	private Middlebox dst;
	private PerflowKey key;
	private Scope scope;
	private Guarantee guarantee;
	private Optimization optimization;
	private short inPort;
		
	// Operation state
    private boolean getPerflowAcked;
    private boolean getMultiflowAcked;
    private volatile int getPerflowCount;
    private volatile int getMultiflowCount;
    private volatile int putPerflowAcks;
    private volatile int putMultiflowAcks;
	private boolean bufferingEnabled;
	private boolean performedPhaseTwo;
	private byte[] lastPacket;
	private boolean immediateRelease;
	
	// State and event processing
	private ConcurrentLinkedQueue<ReprocessEvent> eventsList;
	private ConcurrentLinkedQueue<StateChunk> statesList;
    private ExecutorService threadPool;
	
	// Statistics
	private boolean isFirstStateRcvd;
	private long moveStart;
	private int srcEventCount;
        
	private int dstEventCount;
	private long getstart;
        int port;  
        NetTelnet dsttfm;
        NetTelnet srctfm;
        
	private volatile int packetCount;
	
	public MoveOperation(IOperationManager manager, Middlebox src, 
			Middlebox dst, PerflowKey key, Scope scope, Guarantee guarantee, 
			Optimization optimization, short inPort)
	{
		// Store arguments
		super(manager);
		this.src = src;
		this.dst = dst;
		this.key = key;
		this.scope = scope;
		this.guarantee = guarantee;
		this.optimization = optimization;
		this.inPort = inPort;
		
		// Initialize operation state variables
		this.getPerflowAcked = false;
		this.getMultiflowAcked = false;
		this.getPerflowCount = 0;
		this.getMultiflowCount = 0;
		this.putPerflowAcks = 0;
		this.putMultiflowAcks = 0;
		this.bufferingEnabled = false;
		this.performedPhaseTwo = false;
		this.lastPacket = null;
		this.immediateRelease = false;
		
		// Initialize state and event processing variables
		this.eventsList  = new ConcurrentLinkedQueue<ReprocessEvent>();
		this.statesList = new ConcurrentLinkedQueue<StateChunk>();
		this.threadPool = Executors.newCachedThreadPool();
		
		// Initialize statistics variables
                this.isFirstStateRcvd = false;
		this.moveStart = -1;
		this.srcEventCount = 0;
		this.dstEventCount = 0;
		this.packetCount = 0;
		tfmInit();
	}

		
    @Override
    public int execute()
    {
		if ((this.guarantee != Guarantee.NO_GUARANTEE) 
				&& (this.optimization == Optimization.NO_OPTIMIZATION 
						|| this.optimization == Optimization.PZ))
		{
  			log.info("enable event on src");
			return this.getId();
		}
		else
	     	{
	            int i = 0;
		    tfm();
                    i=issueGet();
		    //tfm_installtable("/root/project/code/tfm/controller_stable/apps/tfm_flow_2.sh");
		    //String r2 = dsttfm.sendCommand("write tfm.stateinstall 1");
		    //log.info("Update flowtable");
    		    /*
		    while(true)
		    {
		        String r1 = dsttfm.sendCommand("read tfm.state");
			if(r1.indexOf("S_INSD_RCVLINFLY")>=0)
			    break;
		    }
                    */
   		    return i;
                }
    }
	private void tfmInit()
        {
	    log.info("TFM Init\n");
	    this.port = 4433;
	    //tfm_installtable("/root/project/code/tfm/controller_stable/apps/tfm_flow_clean.sh");
 	    this.dsttfm = new NetTelnet(this.dst.getManagementIP(), port);
 	    this.srctfm = new NetTelnet(this.src.getManagementIP(), port);
	    //tfm_installtable("/root/project/code/tfm/controller_stable/apps/tfm_flow_1.sh");
            //String r1 = srctfm.sendCommand("write srcredirect.pattern1 10.10.10.10/32");
            //r1 = dsttfm.sendCommand("write srcredirect.pattern1 10.10.10.10/32");
	    //tfm_installtable("/root/project/code/tfm/controller_stable/apps/tfm_flow.sh");
            return;
        }
        private void tfm_installtable(String command)
        {
	    try {
                Process process = Runtime.getRuntime().exec(command);
	    }
	    catch (IOException e) {
	    }
            return;
	}
	private void tfm()
        {
	    log.info("TFM start to migrate");
	    this.moveStart = System.currentTimeMillis();
            String r1 = dsttfm.sendCommand("write tfm.mfstart 1");
            //update flowtable ,send from src and dst 
            //r1 = srctfm.sendCommand("write eth.src e8:61:1f:10:6c:d1");
            //r1 = srctfm.sendCommand("write eth.dst e8:61:1f:10:6d:53");
            srctfm.commitCommand("write srcredirect.pattern0 10.10.10.10/32");
            tfm_installtable("/root/project/code/tfm/controller_stable/apps/tfm_flow_2.sh");
	    //tfm_installtable("/root/project/code/tfm/controller_stable/apps/tfm_redirect.sh");
            //update flowtable ,send to src and dst 
            //update flowtable ,send to dst 
        }
	private int issueGet()
	{
		GetPerflowMessage getPerflow = null;
		GetMultiflowMessage getMultiflow = null;
		if (Scope.PERFLOW == this.scope || Scope.PF_MF == this.scope)
		{
			boolean raiseEvents = true;
			if (this.guarantee == Guarantee.NO_GUARANTEE 
					|| this.optimization == Optimization.NO_OPTIMIZATION 
					|| this.optimization == Optimization.PZ)
			{  raiseEvents = false; } 	
			getPerflow = new GetPerflowMessage(this.getId(), this.key, raiseEvents);
			log.info("Start get perflow");
		        this.getstart = System.currentTimeMillis();
		        
    		try
    		{ this.src.getStateChannel().sendMessage(getPerflow); }
    		catch(MessageException e)
    		{ return -1; }
		}
		if (Scope.MULTIFLOW == this.scope || Scope.PF_MF == this.scope)
		{
			getMultiflow = new GetMultiflowMessage(this.getId(), this.key);
    		try
    		{ this.src.getStateChannel().sendMessage(getMultiflow); }
    		catch(MessageException e)
    		{ return -1; }
		}
		
		if (null == getPerflow && null == getMultiflow)
		{
			log.error("Scope needs to be 'PERFLOW', 'MULTIFLOW' or 'PF_MF'");
			return -1; 
		}

        return this.getId();
    }

	@Override
	public void receiveStatePerflow(StatePerflowMessage msg) 
	{ this.receiveStateMessage(msg); }

    @Override
    public void receiveStateMultiflow(StateMultiflowMessage msg) 
    { this.receiveStateMessage(msg); }
    
    public void receiveMigrateFinishAck(MigrateFinishAckMessage msg) 
    {
        long moveEnd = System.currentTimeMillis();
        long moveTime = moveEnd - this.moveStart;
        log.info(String.format("here[MOVE_TIME] elapse=%d, start=%d, end=%d",moveTime, this.moveStart, moveEnd));
    }
    private void receiveStateMessage(StateMessage msg)
    {
	
    	if (!this.isFirstStateRcvd) 
    	{
    		//this.moveStart = System.currentTimeMillis();
        	log.info("Move Start ({})", this.getId());
        	this.isFirstStateRcvd = true;
    	}
    	
        // Add state chunk to list of state associated with this operation
        StateChunk chunk = obtainStateChunk(msg.hashkey);
    	chunk.storeStateMessage(msg, this.dst);
       
        if (optimization == Optimization.NO_OPTIMIZATION 
        		|| optimization == Optimization.LL)
        { this.statesList.add(chunk); }
        else
        {
        	this.threadPool.submit(chunk);
        	//chunk.call();
        }
    }

    @Override
    public void receivePutPerflowAck(PutPerflowAckMessage msg) 
    {
        this.putPerflowAcks++;
        this.receivePutAck(msg);
    }
    
    @Override
    public void receivePutMultiflowAck(PutMultiflowAckMessage msg) 
    {
    	this.putMultiflowAcks++;
        this.receivePutAck(msg);
    }
    
    private void receivePutAck(PutAckMessage msg)
    {
        StateChunk chunk = obtainStateChunk(msg.hashkey);
        
        // Indicate state chunk has been acked
        chunk.receivedPutAck();
        checkIfStateMoved();
    }

	@Override
	public void receiveReprocess(ReprocessMessage msg, Middlebox from) 
	{	
	}
	
	@Override
    public void receiveGetPerflowAck(GetPerflowAckMessage msg) 
    {
	log.info("Completed state per-flow state get");
        this.getPerflowAcked = true;
        this.getPerflowCount = msg.count;
        if (optimization == Optimization.NO_OPTIMIZATION || optimization == Optimization.LL)
        {
        	if (this.scope == Scope.PF_MF && this.getMultiflowAcked == false)
        	{ return; }        	
        	releaseBufferedStates();
        }
        else
        { checkIfStateMoved(); }
    }

    @Override
    public void receiveGetMultiflowAck(GetMultiflowAckMessage msg) 
    {
    	log.info("Completed state multi-flow state get");
        this.getMultiflowAcked = true;
        this.getMultiflowCount = msg.count;
        if (optimization == Optimization.NO_OPTIMIZATION || optimization == Optimization.LL)
        {
        	if (this.scope == Scope.PF_MF && this.getPerflowAcked == false)
        	{ return; }
        	releaseBufferedStates();
        }
        else
        { checkIfStateMoved(); }
    }
    
    private void releaseBufferedStates()
    {
    	if (!this.statesList.isEmpty())
		{
			//if we are here, this operation does not use early release optimization
			// release the globally held events
			try 
			{ this.threadPool.invokeAll(this.statesList); }
			catch (InterruptedException e) 
			{
				log.error("Failed to release all events");
				this.fail();
				return;
			}
		}
    }
    
	private void checkIfStateMoved()
	{
		boolean completed = false;
		switch (this.scope)
		{
			case PERFLOW:
				completed = (this.getPerflowAcked 
						&& (this.putPerflowAcks == this.getPerflowCount));
				break;
			case MULTIFLOW:
				completed = (this.getMultiflowAcked 
						&& (this.putMultiflowAcks == this.getMultiflowCount));
				break;
			case PF_MF:
				completed = (this.getPerflowAcked && this.getMultiflowAcked 
						&& (this.putPerflowAcks == this.getPerflowCount) 
						&& (this.putMultiflowAcks == this.getMultiflowCount));
				break;
		}
		
		if (completed)
		{
			long getEnd = System.currentTimeMillis();
			this.getstart = getEnd - this.getstart;
		        String r1 = dsttfm.sendCommand("write tfm.stateinstall 1");
			log.info("Completed all state transfer");
			log.info(String.format("state move time=%d",
					this.getstart));
			/*while (!this.eventsList.isEmpty())
			{ 
				//if we are here, this operation does not use early release optimization
				//release the globally held events
				this.eventsList.poll().process();
			}*/
			
			if (!this.eventsList.isEmpty())
			{
				//if we are here, this operation does not use early release optimization
				// release the globally held events
				if (this.guarantee == Guarantee.ORDER_PRESERVING)
				{
					
					while(!this.eventsList.isEmpty())
					{
						ReprocessEvent event = this.eventsList.remove();
						Future<Boolean> future = this.threadPool.submit(event);
						try 
						{ future.get(); }
						catch (Exception e) 
						{
							log.error("Failed to release event");
							this.fail();
							return;
						}
					}
				}
				else
				{
					try 
					{ this.threadPool.invokeAll(this.eventsList); }
					catch (InterruptedException e) 
					{
						log.error("Failed to release all events");
						this.fail();
						return;
					}
				}
			}
			
	        if (this.guarantee != Guarantee.ORDER_PRESERVING)
	        {
		    log.info("End Move ({})", this.getId());
	            this.finish();
	        }
			this.immediateRelease = true;
		}
	}
	
	@Override
	public void receiveEventsAck(EventsAckMessage msg, Middlebox from)
	{
		if (from == this.src)
		{ 
			issueGet();
			return;
		}
		// If buffering was not enabled, then this must be the ACK for the
		// request to enable it
		if (!this.bufferingEnabled)
		{
		}
		else
		{
			//This is the ack for disable events at destination
			// FIXME: Is it safe to assume we are done?
			log.info("End Move");
			log.info("#events = " + this.srcEventCount);
			log.info("#packets = " + this.packetCount);

			long moveEnd = System.currentTimeMillis();
			long moveTime = moveEnd - this.moveStart;
			log.info(String.format("[MOVE_TIME] elapse=%d, start=%d, end=%d",
					moveTime, this.moveStart, moveEnd));
			log.info(String.format("state move time=%d",
					this.getstart));

			this.finish();
		}
	}
	
	@Override
	public void receivePacket(byte[] packet, IOFSwitch inSwitch, short inPort) 
	{
		// Ignore packets from unexpected switch ports
		if ((this.inPort != inPort) || (this.src.getSwitch() != inSwitch))
		{
			log.debug(String.format("[Move %d] Received packet from wrong switch port", this.getId()));
			return; 
		}
		
		this.packetCount++;
		if(this.lastPacket == null) {
			this.lastPacket = PacketUtil.setFlag(packet, PacketUtil.FLAG_NONE);
			this.lastPacket = packet;
		}
		//log.debug(String.format("[Move %d] Received packet that was sent to src", this.getId()));
		
		// FIXME: We cannot send to src & controller because HP switches
		// suck, so we must send to controller and then to src
		/*OFPacketOut send = new OFPacketOut();
		ArrayList<OFAction> actions = new ArrayList<OFAction>();
        OFAction outputTo = new OFActionOutput(this.src.getSwitchPort());
        actions.add(outputTo);
        send.setActions(actions);
        send.setActionsLength((short)OFActionOutput.MINIMUM_LENGTH);
        send.setLength((short)(OFPacketOut.MINIMUM_LENGTH + send.getActionsLength()));
        send.setBufferId(OFPacketOut.BUFFER_ID_NONE);
        send.setPacketData(packet);
        send.setLength((short)(send.getLength() + packet.length));
        try
        {
        	this.src.getSwitch().write(send, null);
        	this.src.getSwitch().flush();
        }
        catch (Exception e)
        {
        	log.error(String.format("[Move %d] Could not flush packet to src", this.getId()));
        	this.fail();
        	return;
        }*/
		
		// Perform phase 2 of the route update once we get one packet
		if (!this.performedPhaseTwo && this.packetCount >= 1)
		{
			this.performedPhaseTwo = true;
			log.debug(String.format("[Move %d] Performed route update phase 2", this.getId()));
			log.info(String.format("last infly=%d",
					this.srcEventCount));
		}
	}
	
}
