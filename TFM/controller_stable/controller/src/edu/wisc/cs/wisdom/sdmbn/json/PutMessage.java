package edu.wisc.cs.wisdom.sdmbn.json;

public abstract class PutMessage extends Message 
{
	public int hashkey;
	public String state;
	
	public PutMessage(String type)
	{ super(type); }
	
	public PutMessage(AllFieldsMessage msg)
	{
		super(msg); 
		this.hashkey = msg.hashkey;
		this.state = msg.state;
	}
	
	public abstract String toString();
}
