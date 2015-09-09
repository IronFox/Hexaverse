using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Net.Sockets;
using System.Threading;
using System.Diagnostics;

namespace Consensus
{
    public static class TimeServer
    {
        static UdpClient client = null;

        static Thread listenThread = null;
        static volatile bool closeDown = false;

        static void Listen()
        {
            System.Net.IPEndPoint remote = new System.Net.IPEndPoint(0,0);
            byte[] outPackage = new byte[16];
            try
            {
                while (!closeDown)
                {
                    try
                    {
                        byte[] data = client.Receive(ref remote);
                        Debug.Assert(data.Length == 8);
                        Array.Copy(data, outPackage, 8);
                        long time = NetTime.Query();
                        outPackage[8] = (byte)(time & 0xFF);
                        outPackage[9] = (byte)((time >> 8) & 0xFF);
                        outPackage[10] = (byte)((time >> 16) & 0xFF);
                        outPackage[11] = (byte)((time >> 24) & 0xFF);
                        outPackage[12] = (byte)((time >> 32) & 0xFF);
                        outPackage[13] = (byte)((time >> 40) & 0xFF);
                        outPackage[14] = (byte)((time >> 48) & 0xFF);
                        outPackage[15] = (byte)((time >> 56) & 0xFF);
                        client.Send(outPackage, 16, remote);
                    }
                    catch (System.Net.Sockets.SocketException)
                    {
                        if (closeDown)
                            return;
                    }
                }
            }
            catch (Exception)
            {}
        }


        public static void Start(int port)
        {
            Debug.Assert(client == null);

            client = new UdpClient(port);
            listenThread = new Thread(new ThreadStart(Listen));
            closeDown = false;
            listenThread.Start();
        }


        public static void End()
        {
            if (client != null)
            {
                closeDown = true;
                client.Close();
                client = null;
                listenThread.Join();
                listenThread = null;
            }
        }

    }
}
