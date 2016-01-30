package edu.wisc.cs.wisdom.sdmbn.json;

public class MigrateFinishAckMessage extends Message 
{
	public int count;
	
	public MigrateFinishAckMessage()
	{ super(Message.RESPONSE_MIGRATE_FINISH_ACK); }
	
	public MigrateFinishAckMessage(AllFieldsMessage msg) throws MessageException
	{
		super(msg);
		if (!msg.type.equals(Message.RESPONSE_MIGRATE_FINISH_ACK))
		{
			throw new MessageException(
					String.format("Cannot construct %s from message of type %s",
							this.getClass().getSimpleName(), msg.type), msg); 
		}
		this.count = msg.count;
	}
	
	public String toString()
	{ return "{id="+id+",type=\""+type+"\",count="+count+"}"; }
}
