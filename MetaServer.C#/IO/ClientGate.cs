using System;
using System.Collections.Concurrent;
using System.Collections.Generic;
using System.Linq;
using System.Text;

namespace Consensus
{
    public class BlockException : Exception
    {
        public BlockException(string message)
            : base(message)
        { }
    }

    public class ClientGate
    {
        private struct ClientInfo
        {
            public long LastEvent;
            public bool PermanentlyBlocked;
            public int BlockExtended;
        }

        private ConcurrentDictionary<string, ClientInfo> lastEvent = new ConcurrentDictionary<string, ClientInfo>();
        private float minDelta = 1.0f;

        public ClientGate(float minDelta)
        {
            this.minDelta = minDelta;
        }

        public void Pass(Client client)
        {
            ClientInfo info;
            if (!lastEvent.TryGetValue(client.GateKey, out info))
            {
                info = new ClientInfo { LastEvent = LocalTime.Query(), PermanentlyBlocked = false, BlockExtended = 0 };
                lastEvent[client.GateKey] = info;
                return;
            }
            if (info.PermanentlyBlocked)
                throw new BlockException("Client is permanently blocked");
            bool blocked = LocalTime.ToSeconds(LocalTime.Query() - info.LastEvent) < minDelta;
            info.LastEvent = LocalTime.Query();
            if (blocked)
            {
                info.BlockExtended++;
                if (info.BlockExtended > 3)
                    info.PermanentlyBlocked = true;
            }
            else
                info.BlockExtended = 0;

            lastEvent[client.GateKey] = info;
            if (blocked)
            {
                if (info.PermanentlyBlocked)
                    throw new BlockException("Client is permanently blocked");
                else
                    throw new BlockException("Client is temporarily blocked");
            }
        }
    }
}
