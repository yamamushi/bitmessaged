//~~ void sendVerAck(ACF_Message* event) [OutSocketHandler] ~~
protocol::message aMessage(protocol::message::verack,protocol::Payload());

if (theKnowledge.getDebug())
{
    printf("Message type Verack:\n");
    dumpSend();
    aMessage.dump();
}

aMessage.writeTo(socketfd);