using System;
using System.Collections;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Diagnostics;
using System.Drawing;
using System.Linq;
using System.Net.Sockets;
using System.Text;
using System.Threading;
using System.Threading.Tasks;
using System.Windows.Forms;

namespace LogDisplay
{
    public partial class LogWindow : Form
    {
        public LogWindow()
        {
            InitializeComponent();
        }

        private TcpClient   client;
        private Thread   listenThread;
        private static System.Text.ASCIIEncoding decoder = new System.Text.ASCIIEncoding();
        private static ToolTip tooltip = new ToolTip();

        struct AddressState
        {
            public int  lastAvatarID,
                        lastAccountID,
                        lastInstanceID;
        };

        class ListViewNF : System.Windows.Forms.ListView
        {
            public ListViewNF()
            {
                //Activate double buffering
                this.SetStyle(ControlStyles.OptimizedDoubleBuffer | ControlStyles.AllPaintingInWmPaint, true);

                //Enable the OnNotifyMessage event so we get a chance to filter out 
                // Windows messages before they get to the form's WndProc
                this.SetStyle(ControlStyles.EnableNotifyMessage, true);
            }

            protected override void OnNotifyMessage(Message m)
            {
                //Filter out the WM_ERASEBKGND message
                if (m.Msg != 0x14)
                {
                    base.OnNotifyMessage(m);
                }
            }
        }

        //http://support.microsoft.com/kb/319401
        public class ListViewColumnSorter : IComparer
        {
            /// <summary>
            /// Specifies the column to be sorted
            /// </summary>
            private int ColumnToSort;
            /// <summary>
            /// Specifies the order in which to sort (i.e. 'Ascending').
            /// </summary>
            private SortOrder OrderOfSort;
            /// <summary>
            /// Case insensitive comparer object
            /// </summary>
            private CaseInsensitiveComparer ObjectCompare;

            /// <summary>
            /// Class constructor.  Initializes various elements
            /// </summary>
            public ListViewColumnSorter()
            {
                // Initialize the column to '0'
                ColumnToSort = 0;

                // Initialize the sort order to 'none'
                OrderOfSort = SortOrder.None;

                // Initialize the CaseInsensitiveComparer object
                ObjectCompare = new CaseInsensitiveComparer();
            }

            /// <summary>
            /// This method is inherited from the IComparer interface.  It compares the two objects passed using a case insensitive comparison.
            /// </summary>
            /// <param name="x">First object to be compared</param>
            /// <param name="y">Second object to be compared</param>
            /// <returns>The result of the comparison. "0" if equal, negative if 'x' is less than 'y' and positive if 'x' is greater than 'y'</returns>
            public int Compare(object x, object y)
            {
                int compareResult;
                ListViewItem listviewX, listviewY;

                // Cast the objects to be compared to ListViewItem objects
                listviewX = (ListViewItem)x;
                listviewY = (ListViewItem)y;

                // Compare the two items
                compareResult = ObjectCompare.Compare(listviewX.SubItems[ColumnToSort].Text, listviewY.SubItems[ColumnToSort].Text);

                // Calculate correct return value based on object comparison
                if (OrderOfSort == SortOrder.Ascending)
                {
                    // Ascending sort is selected, return normal result of compare operation
                    return compareResult;
                }
                else if (OrderOfSort == SortOrder.Descending)
                {
                    // Descending sort is selected, return negative result of compare operation
                    return (-compareResult);
                }
                else
                {
                    // Return '0' to indicate they are equal
                    return 0;
                }
            }

            /// <summary>
            /// Gets or sets the number of the column to which to apply the sorting operation (Defaults to '0').
            /// </summary>
            public int SortColumn
            {
                set
                {
                    ColumnToSort = value;
                }
                get
                {
                    return ColumnToSort;
                }
            }

            /// <summary>
            /// Gets or sets the order of sorting to apply (for example, 'Ascending' or 'Descending').
            /// </summary>
            public SortOrder Order
            {
                set
                {
                    OrderOfSort = value;
                }
                get
                {
                    return OrderOfSort;
                }
            }

        }

        ListViewColumnSorter lvwColumnSorter;

        ListViewNF eventList;

        private Dictionary<string, AddressState> stateHistory = new Dictionary<string, AddressState>();

        private void MakeBold(ListViewItem.ListViewSubItem item, FontStyle style)
        {
            item.ForeColor = Color.Navy;
            item.Font = new Font(FontFamily.GenericSansSerif, 8, style | FontStyle.Bold);
            if (item.Text.Length == 0)
                item.Text = "-";
        }

        public enum Significance
        {
            Low,
            Common,
            Unusual,
            Important,
            ClientFatal,
            ProgramFatal,

            LogEvent
        }

        static private int eventCounter = 0;

        private void AddRow(string dateTime, string significance, string remoteIP, string remoteAccountID, string remoteAccountName, string remoteAvatarID, string remoteAvatarName, string remoteInstanceID, string message)
        {

            int red=0;
            int green = 0;
            int blue = 0;

            Color backColor = Color.White;
            FontStyle style = FontStyle.Regular;
            Significance sig = (Significance)Enum.Parse(typeof(Significance), significance);
            switch (sig)
            {
                case Significance.Low:
                    red = green = blue = 128;
                break;
                default:
                break;
                case Significance.Unusual:
                    red = 128;
                    style |= FontStyle.Italic;
                break;
                case Significance.Important:
                    style |= FontStyle.Bold;
                break;
                case Significance.ClientFatal:
                    red = 192;
                    style |= FontStyle.Bold;
                    style |= FontStyle.Italic;
                break;
                case Significance.ProgramFatal:
                    red = 255;
                    backColor = Color.FromArgb(48, 32, 96);
                    style |= FontStyle.Bold;
                    style |= FontStyle.Underline;
                    style |= FontStyle.Italic;
                break;
                case Significance.LogEvent:
                    blue = 128;
                    style |= FontStyle.Bold;
                break;
            }

            Font font = new Font(FontFamily.GenericSansSerif, 8, style);
            Color foreColor = Color.FromArgb(red, green, blue);
            eventCounter++;
            string counterString = eventCounter.ToString();
            while (counterString.Length < 8)
                counterString = "0" + counterString;


            ListViewItem item = new ListViewItem(new string[] { counterString, dateTime, message, remoteIP, remoteAccountName, remoteAvatarName, remoteInstanceID }, 0, foreColor, backColor, null);
            item.UseItemStyleForSubItems = false;
            item.SubItems[0].ForeColor = backColor;
            for (int i = 1; i < item.SubItems.Count; i++)
            {
                if (i == 2)
                {
                    item.SubItems[i].Font = font;
                }
                item.SubItems[i].ForeColor = foreColor;
                item.SubItems[i].BackColor = backColor;
                
            }

            if (remoteIP.Length > 0)
            {
                bool avatarChanged = false;
                bool accountChanged = false;
                bool instanceChanged = false;
                int avID = remoteAvatarID.Length > 0 ? int.Parse(remoteAvatarID) : 0;
                int accID = remoteAccountID.Length > 0 ? int.Parse(remoteAccountID) : 0;
                int iID = remoteInstanceID.Length > 0 ? int.Parse(remoteInstanceID) : 0;
                AddressState lastState;
                if (stateHistory.TryGetValue(remoteIP, out lastState))
                {
                    accountChanged = lastState.lastAccountID != accID;
                    avatarChanged = lastState.lastAvatarID != avID;
                    instanceChanged = lastState.lastInstanceID != iID;
                }
                else
                {
                    avatarChanged = avID != 0;
                    accountChanged = accID != 0;
                    instanceChanged = iID != 0;

                }
                if (accountChanged)
                    MakeBold(item.SubItems[4], style);
                if (avatarChanged)
                    MakeBold(item.SubItems[5],style);
                if (instanceChanged)
                    MakeBold(item.SubItems[6], style);
                if (instanceChanged || avatarChanged || accountChanged)
                {
                    lastState = new AddressState { lastAccountID = accID, lastAvatarID = avID, lastInstanceID = iID };
                    stateHistory[remoteIP] = lastState;
                }
            }
            int index = eventList.Items.Count;
            eventList.Items.Add(item);
            eventList.Sort();
            int topItem = eventList.Items.IndexOf(eventList.TopItem);
            int numRows = (eventList.ClientSize.Height) / (font.Height + 3) -1;
            int bottomItem = topItem + numRows;
            if (bottomItem + 1 > eventList.Items.Count)
                eventList.EnsureVisible(index);
            //ListViewItem.ListViewSubItemCollection rowItems = eventList.Items.Add(dateTime).SubItems;
            //rowItems.Add(message);
            //rowItems.Add(remoteIP);
            //rowItems.Add(remoteAccountName);
            //rowItems.Add(remoteAvatarName);
        }


        string[] arguments;
        public delegate void SubLogT();
        private void SubLog()
        {
            string[] p = (string[])arguments;
            Debug.Assert(p.Length == 9);
            AddRow(p[0],p[1],p[2],p[3],p[4],p[5],p[6],p[7],p[8]);
        }


        private void LogEvent(string[] arguments)
        {
            this.arguments = arguments;
            try
            {
                eventList.Invoke(new SubLogT(SubLog));
            }
            catch
            { }
        }


        private void ProcessString(byte[] buffer, int start, int count)
        {
            string str = decoder.GetString(buffer, start, count);

            string[] tokens = str.Split(new char[1] { '\t' });
            Debug.Assert(tokens.Length == 9);

            LogEvent(tokens);
            //LogEvent(tokens[0], tokens[1], tokens[2], tokens[3], tokens[4], tokens[5], tokens[6], tokens[7], tokens[8]);
        }

        private void Listen()
        {
            NetworkStream   stream = client.GetStream();
            byte[] buffer = new byte[2048];
            int offset = 0;
            LogEvent(new string[] { System.DateTime.Now.ToString(), "LogEvent", "", "", "", "", "", "", "Connection to server established" });
            while (client != null)
            {
                try
                {
                    int read = stream.Read(buffer, offset, 2048 - offset);
                    if (read <= 0)
                        throw new Exception();
                    int lineStart = 0;

                    for (int i = offset; i < offset + read; i++)
                        if (buffer[i] == 0)
                        {
                            ProcessString(buffer, lineStart, i - lineStart);
                            lineStart = i + 1;
                        }
                    if (lineStart < offset + read)
                    {
                        Array.Copy(buffer, lineStart, buffer, 0, offset + read - lineStart);
                        offset = offset + read - lineStart;
                    }
                    else
                        offset = 0;
                }
                catch
                {
                    //client.Close();
                    LogEvent(new string[] { System.DateTime.Now.ToString(), "LogEvent", "", "", "", "", "", "", "Connection to server lost or closed" });
                    return;
                }
            }
        }


        private bool TryConnect()
        {
            menuDisconnect_Click(null,null);


            try
            {
                client = new TcpClient();
                client.Connect("localhost", 12879);
                listenThread = new Thread(new ThreadStart(Listen));
                listenThread.Start();
                return true;
            }
            catch
            {
                return false;
            }
        }

        private void menuConnect_Click(object sender, EventArgs e)
        {
            while (true)
            {
                if (TryConnect())
                    return;
                DialogResult result =
                MessageBox.Show("Connection to localhost has failed. Server is unresponsive.", "Error", MessageBoxButtons.RetryCancel, MessageBoxIcon.Error);
                if (result != DialogResult.Retry)
                    return;
            }
        }

        private void menuDisconnect_Click(object sender, EventArgs e)
        {
            try
            {
                if (client != null)
                {
                    client.GetStream().Close();
                    client.Close();
                    client = null;
                }
            }
            catch { }
        }

        private void LogWindow_FormClosing(object sender, FormClosingEventArgs e)
        {
            menuDisconnect_Click(null, null);
            Application.Exit();
        }

        private void menuQuit_Click(object sender, EventArgs e)
        {
            menuDisconnect_Click(null, null);
            this.Close();
            Application.Exit();

        }

        private void menuClearHistory_Click(object sender, EventArgs e)
        {
            eventList.Items.Clear();
        }

        private void eventList_Click(object sender, ColumnClickEventArgs e)
        {
            // Determine if clicked column is already the column that is being sorted.
            if (e.Column == lvwColumnSorter.SortColumn)
            {
                // Reverse the current sort direction for this column.
                if (lvwColumnSorter.Order == SortOrder.Ascending)
                {
                    lvwColumnSorter.Order = SortOrder.Descending;
                }
                else
                {
                    lvwColumnSorter.Order = SortOrder.Ascending;
                }
            }
            else
            {
                // Set the column number that is to be sorted; default to ascending.
                lvwColumnSorter.SortColumn = e.Column;
                lvwColumnSorter.Order = SortOrder.Ascending;
            }

            // Perform the sort with these new sort options.
            eventList.Sort();
        }

        private void LogWindow_Load(object sender, EventArgs e)
        {
            eventList = new ListViewNF();
            //eventList.Dock = DockStyle.Fill;
            eventList.Columns.Add("#", 10).AutoResize(ColumnHeaderAutoResizeStyle.HeaderSize);
            eventList.Columns.Add("DateTime", 115);
            eventList.Columns.Add("", 400);
            eventList.Columns.Add("Address", 120);
            eventList.Columns.Add("Account", 90);
            eventList.Columns.Add("Avatar", 90);
            eventList.Columns.Add("IID", 32);
            //var margin = eventList.Margin;
            //margin.Top = menuStrip1.Height;
            //eventList.Margin = margin;
            eventList.Top = menuStrip1.Height;
            eventList.Left = 0;
            eventList.Width = ClientSize.Width;
            eventList.Height = ClientSize.Height - eventList.Top;
            eventList.FullRowSelect = true;
            eventList.Sorting = SortOrder.Descending;
            eventList.ColumnClick += eventList_Click;
            lvwColumnSorter = new ListViewColumnSorter();
            eventList.ListViewItemSorter = lvwColumnSorter;
            //eventList.AutoArrange = true;
            eventList.Anchor = AnchorStyles.Top | AnchorStyles.Left | AnchorStyles.Right | AnchorStyles.Bottom;
            eventList.View = View.Details;
            Controls.Add(eventList);

        }
    }
}
