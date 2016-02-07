package edu.wisc.cs.wisdom.sdmbn.apps.testing;
import java.io.IOException;
import java.util.Arrays;
import java.util.List;
import java.util.Map;

import net.floodlightcontroller.core.module.FloodlightModuleContext;
import net.floodlightcontroller.core.module.FloodlightModuleException;
import net.floodlightcontroller.packet.Ethernet;
import net.floodlightcontroller.packet.IPv4;

import edu.wisc.cs.wisdom.sdmbn.Middlebox;
import edu.wisc.cs.wisdom.sdmbn.Parameters.Guarantee;
import edu.wisc.cs.wisdom.sdmbn.Parameters.Optimization;
import edu.wisc.cs.wisdom.sdmbn.PerflowKey;
import java.io.InputStream;
import java.io.PrintStream;
import org.apache.commons.net.telnet.TelnetClient;

class TelNet {
	private TelnetClient telnet = new TelnetClient();
	private InputStream in;
	private PrintStream out;
	private char prompt = '$';

	// 普通用户结束
	public TelNet(String ip, int port) {
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

	/** * 读取分析结果 * * @param pattern * @return */
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

	/** * 写操作 * * @param value */
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

	/** * 关闭连接 */
	public void disconnect() {
		try {
			telnet.disconnect();
		} catch (Exception e) {
			e.printStackTrace();
		}
	}
}



public class TestTimedMoveAll extends TestTimed
{
	private Guarantee guarantee;
	private Optimization optimization;

	@Override
	protected void parseArguments(FloodlightModuleContext context)
			throws FloodlightModuleException 
	{
		super.parseArguments(context);
		
		// Get arguments specific to move
		Map<String,String> config = context.getConfigParams(this);
		
		this.checkForArgument(config, "Guarantee");
		this.guarantee = Guarantee.valueOf(config.get("Guarantee"));
		log.debug(String.format("Guarantee = %s", this.guarantee.name()));
		
		this.checkForArgument(config, "Optimization");
		this.optimization = Optimization.valueOf(config.get("Optimization"));
		log.debug(String.format("Optimization = %s", this.optimization.name()));
	}
	
	@Override
	protected void initialRuleSetup()
	{
		Middlebox mb1 = middleboxes.get("mb1");
		synchronized(this.activeMbs)
		{ this.activeMbs.add(mb1); }
	
		List<Middlebox> mbs = Arrays.asList(new Middlebox[]{mb1});
		tfm_init();
		//this.changeForwarding(new PerflowKey(), mbs);
	}
        protected void tfm_init()
        {
	    Middlebox src = middleboxes.get("mb1");
	    Middlebox dst = middleboxes.get("mb2");
	    int port = 4433;
	    log.info("Environment Init\n");
            tfm_installtable("/root/project/code/tfm/controller_stable/apps/tfm_flow_clean.sh");
            TelNet dsttfm = new TelNet(dst.getManagementIP(), port);
            TelNet srctfm = new TelNet(src.getManagementIP(), port);
            String r1 = srctfm.sendCommand("write srcredirect.pattern1 10.10.10.10/32");
            r1 = dsttfm.sendCommand("write srcredirect.pattern1 10.10.10.10/32");
	    r1 = srctfm.sendCommand("write eth.src e8:61:1f:10:6c:d1");
            r1 = srctfm.sendCommand("write eth.dst e8:61:1f:10:6d:53");
	    srctfm.disconnect();
	    dsttfm.disconnect();
            tfm_installtable("/root/project/code/tfm/controller_stable/apps/tfm_flow.sh");
	    tfm_installtable("/root/project/code/tfm/controller_stable/apps/tfm_flow_1.sh");
	}
        protected void tfm_installtable(String command)
        {
            try {
                Process process = Runtime.getRuntime().exec(command);
            }
            catch (IOException e) {
            }
            return;
        }
	
	@Override
	protected void initiateOperation()
	{
		Middlebox mb1 = middleboxes.get("mb1");
		Middlebox mb2 = middleboxes.get("mb2");
		synchronized(this.activeMbs)
		{ this.activeMbs.add(mb2); }
				
		PerflowKey key = new PerflowKey();
		key.setDlType(Ethernet.TYPE_IPv4);
		//key.setNwProto(IPv4.PROTOCOL_TCP);
		key.setNwProto(IPv4.PROTOCOL_UDP);
		log.info("Key="+key);
		int moveOpId = sdmbnProvider.move(mb1, mb2, key, this.scope, 
				this.guarantee, this.optimization, this.traceSwitchPort);
		if (moveOpId >= 0)
		{ log.info("Initiated move"); }
		else
		{ log.error("Failed to initiate move"); }
	}

	@Override
	protected void terminateOperation()
	{ return; }
}
