using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Diagnostics;
using System.Drawing;
using System.Globalization;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Windows.Forms;

namespace ServerManager
{
	public partial class EditServerConfig : Form
	{
		public EditServerConfig()
		{
			InitializeComponent();
		}
		private Action<string> onCompleteHandlePath;
		private string fileName;


		

		abstract class ConfigEntry
		{
			public readonly string Name;
			public readonly string[] Help;
			public readonly Group Parent;

			public abstract Control Control { get; }

			public ConfigEntry(Group parent, string name, string[] help)
			{
				Parent = parent;
				Name = name;
				Help = help;
			}

			protected void WriteHelp(StreamWriter writer)
			{
				if (Help != null)
					foreach (var help in Help)
					{
						Parent.WriteIndention(writer);
						writer.WriteLine(help);
					}
			}

			public abstract void Write(StreamWriter writer);
		};

		class Group : ConfigEntry
		{
			public readonly List<ConfigEntry> Children = new List<ConfigEntry>();
			public readonly int Depth;
			public readonly GroupBox Box;

			public Group(Control parent) : base(null, "", null)
			{
				Depth = 0;
				Box = new GroupBox();
				Box.Text = "Attributes";
				Box.Dock = DockStyle.Top;
				parent.Controls.Add(Box);
				parent.Controls.SetChildIndex(Box, 0);
				Box.Padding = new Padding(10,5,10,5);
			}
			public Group(Group parent, string name, string[] help) : base(parent,name,help)
			{
				Depth = parent.Depth + 1;
				Box = new GroupBox();
				Box.Text = name;
				//Box.Height = Box.PreferredSize.Height;
				parent.AddChild(this);
				////Box.Margin = new Padding(10, 0, 0, 0);
				Box.Padding = new Padding(10,5,10,5);
			}

			public override Control Control => Box;

			public const int LabelWidth = 250;

			public static ToolTip toolTip;

			public void AddChild(ConfigEntry entry)
			{
				Children.Add(entry);

				string tt = entry.Help != null ? string.Join("\n", entry.Help.Select((str) => str.Substring(1))) : "";

				var ctrl = entry.Control;
				toolTip.SetToolTip(ctrl, tt);

				if (!(ctrl is GroupBox))
				{
					var p = new Panel();
					var l = new Label();
					var pt = Box.PointToScreen(Point.Empty);
					p.Width = 1000;
					l.Text = entry.Name;
					l.Size = l.PreferredSize;
					p.Controls.Add(l);
					l.Location = new Point(LabelWidth - pt.X - l.Size.Width, 0);
					p.Controls.Add(ctrl);
					//ctrl.Size = ctrl.PreferredSize;
					ctrl.Width = 1000 - LabelWidth + pt.X;
					
					ctrl.Location = new Point(LabelWidth - pt.X, 0);
					ctrl.Anchor |= AnchorStyles.Right | AnchorStyles.Left | AnchorStyles.Top;
					p.Size = p.PreferredSize;

					toolTip.SetToolTip(l, tt);
					toolTip.SetToolTip(p, tt);

					p.Dock = DockStyle.Top;
					Box.Controls.Add(p);
					Box.Controls.SetChildIndex(p, 0);
				}
				else
				{
					ctrl.Dock = DockStyle.Top;
					Box.Controls.Add(ctrl);
					Box.Controls.SetChildIndex(ctrl, 0);
				}

			}

			public override void Write(StreamWriter writer)
			{
				if (Parent != null)
				{
					WriteHelp(writer);
					Parent.WriteIndention(writer);
					writer.Write('[');
					writer.Write(Name);
					writer.WriteLine(']');
				}

				foreach (var child in Children)
					child.Write(writer);

			}

			public void WriteIndention(StreamWriter writer)
			{
				for (int i = 0; i < Depth; i++)
					writer.Write('\t');
			}
		};
		
		class Attribute : ConfigEntry
		{
			public readonly Control MainControl;
			public Attribute(Group parent, string name, Control valueControl, string[] help) : base(parent, name, help)
			{
				MainControl = valueControl;
				parent.AddChild(this);
			}

			public override Control Control => MainControl;

			public override void Write(StreamWriter writer)
			{
				WriteHelp(writer);
				Parent.WriteIndention(writer);
				writer.Write(Name);
				writer.Write('=');
				writer.WriteLine(StringValue);
			}

			public virtual string StringValue { get { return MainControl.Text; } set { MainControl.Text = value; } }
		};

		class UnsupportedAttribute : Attribute
		{
			private string rawValue;
			private readonly string type;
			public UnsupportedAttribute(Group parent, string name, string type, string[] help) : base(parent, name, new Label(), help)
			{
				this.type = type;
			}

			public override string StringValue { get { return rawValue; } set { rawValue = value; MainControl.Text = value + " (" + type + ")"; } }
		}

		class StringAttribute : Attribute
		{
			public StringAttribute(Group parent, string name, string[] help) : base(parent, name, new TextBox(), help)
			{}
		};

		class IntAttribute : Attribute
		{
			public readonly int MinValue, MaxValue;
			public IntAttribute(Group parent, string name, string range, string[] help) : base(parent, name, new TextBox(), help)
			{
				if (range == null)
					throw new ArgumentNullException("range");
				if (!range.StartsWith("[") || !range.EndsWith("]"))
					throw new ArgumentException("Invalid range: "+range);
				int commaAt = range.IndexOf(',');
				if (commaAt < 0)
					throw new ArgumentException("Invalid range: " + range);

				MinValue = int.Parse(range.Substring(1, commaAt - 1).Trim());
				MaxValue = int.Parse(range.Substring(commaAt +1 , range.Length - commaAt-2).Trim());

				MainControl.KeyPress += new KeyPressEventHandler((obj,ev) =>
				{
					if ((ev.KeyChar < '0' || ev.KeyChar > '9') && (ev.KeyChar != '-' || MinValue >= 0) && ev.KeyChar != 8)
						ev.Handled = true;
				});

				MainControl.TextChanged += new EventHandler((obj, ev) =>
				{
					if (MainControl.Text.Length == 0)
					{
						MainControl.BackColor = Color.Red;
						return;
					}
					try
					{
						int v = int.Parse(MainControl.Text);
						if (v < MinValue || v > MaxValue)
							MainControl.BackColor = Color.Red;
						else
							MainControl.BackColor = SystemColors.Window;
					}
					catch
					{
						MainControl.BackColor = Color.Red;
					}
				});
			}
		};

		class FloatAttribute : Attribute
		{
			public readonly float MinValue, MaxValue;
			public FloatAttribute(Group parent, string name, string range, string[] help) : base(parent, name, new TextBox(), help)
			{
				if (range == null)
					throw new ArgumentNullException("range");
				if (!range.StartsWith("[") || !range.EndsWith("]"))
					throw new ArgumentException("Invalid range: " + range);
				int commaAt = range.IndexOf(',');
				if (commaAt < 0)
					throw new ArgumentException("Invalid range: " + range);

				MinValue = float.Parse(range.Substring(1, commaAt - 1).Trim(), CultureInfo.InvariantCulture);
				MaxValue = float.Parse(range.Substring(commaAt + 1, range.Length - commaAt - 2).Trim(), CultureInfo.InvariantCulture);

				MainControl.KeyPress += new KeyPressEventHandler((obj, ev) =>
				{
					if ((ev.KeyChar < '0' || ev.KeyChar > '9') && (ev.KeyChar != '.') && (ev.KeyChar != '-' || MinValue >= 0) && ev.KeyChar != 8)
						ev.Handled = true;
				});

				MainControl.TextChanged += new EventHandler((obj, ev) =>
				{
					if (MainControl.Text.Length == 0)
					{
						MainControl.BackColor = Color.Red;
						return;
					}
					try
					{
						var v = float.Parse(MainControl.Text, CultureInfo.InvariantCulture);
						if (v < MinValue || v > MaxValue)
							MainControl.BackColor = Color.Red;
						else
							MainControl.BackColor = Color.White;
					}
					catch
					{
						MainControl.BackColor = Color.Red;
					}
				});
			}
		};

		class EnumAttribute : Attribute
		{
			public EnumAttribute(Group parent, string name, string range, string[] help) : base(parent, name, new ComboBox(), help)
			{
				var box = (ComboBox)MainControl;
				box.DropDownStyle = ComboBoxStyle.DropDownList;

				if (range == null)
					throw new ArgumentNullException("range");

				var items = range.Split(',');
				foreach (var item in items)
					box.Items.Add(item.Trim());
				//box.SelectedValue = value;
			}

			public override string StringValue { get => base.StringValue;
				set
				{
					var box = (ComboBox)MainControl;
					for (int i = 0; i < box.Items.Count; i++)
						if (box.Items[i].ToString() == value)
						{
							box.SelectedIndex = i;
							break;
						}
				}
			}
		};
		class SetAttribute : Attribute
		{
			private readonly CheckBox[] choices;
			private readonly Dictionary<string, CheckBox> map = new Dictionary<string, CheckBox>();
			public SetAttribute(Group parent, string name, string range, string[] help) : base(parent, name, new Panel(), help)
			{
				var p = (Panel)MainControl;
				if (range.EndsWith("*"))
					range = range.Substring(0, range.Length - 1);
				range = range.Trim();
				if (range.StartsWith("{"))
					range = range.Substring(1, range.Length - 2);
				var strChoices = range.Split(',');

				int offset = 0;
				choices = new CheckBox[strChoices.Length];
				for (int i = 0; i < strChoices.Length; i++)
				{
					var ch = choices[i] = new CheckBox();

					string chName = strChoices[i].Trim();
					if (chName.StartsWith("'") && chName.EndsWith("'"))
						chName = chName.Substring(1, chName.Length - 2);

					ch.Text = chName;
					p.Controls.Add(ch);
					//ch.Dock = DockStyle.Left;
					ch.Size = ch.PreferredSize;
					ch.Location = new Point(offset,0);
					offset += ch.Size.Width;

					map.Add(ch.Text.ToLower(), ch);
				}
				//p.AutoScroll = true;
				//p.AutoSize = true;
				//p.AutoSizeMode = AutoSizeMode.GrowAndShrink;
				//p.Height = p.PreferredSize.Height + 20;
				//p.Height = p.PreferredSize.Height;
			}

			public override string StringValue
			{
				get
				{
					List<String> items = new List<string>();
					foreach (var ch in choices)
						if (ch.Checked)
							items.Add(ch.Text);
					return string.Join(",", items);
				}
				set
				{
					var pieces = value.Split(',');
					foreach (var c in choices)
						c.Checked = false;
					foreach (var p in pieces)
					{
						map[p.Trim().ToLower()].Checked = true;
					}
				}
			}
		};


		class BoolAttribute : Attribute
		{
			public BoolAttribute(Group parent, string name, string[] help) : base(parent, name, new CheckBox(), help)
			{}
			public override string StringValue { get { return ((CheckBox)MainControl).Checked.ToString().ToLower(); } set { ((CheckBox)MainControl).Checked = bool.Parse(value); } }
		};

		private Group root;


		private void Reload()
		{
			attributePanel.Controls.Clear();
			Group.toolTip = toolTip;

			var stack = new List<Group>();
			stack.Add(root = new Group(attributePanel));




			{
				var info = new ProcessStartInfo();
				info.UseShellExecute = false;
				info.CreateNoWindow = true;
				info.FileName = Path.Combine("server", "Server.exe");
				info.Arguments = "-g \"" + fileName+'\"';
				var p = Process.Start(info);
				p.WaitForExit();
			}

			var lines = File.ReadAllLines(fileName);

			var comments = new List<string>();

			foreach (var line in lines)
			{
				int depth = 0;
				for (; depth < line.Length; depth++)
					if (line[depth] != '\t')
						break;
				string content = line.Substring(depth).Trim();
				if (content.Length == 0)
					continue;

				while (depth < stack[stack.Count - 1].Depth)
					stack.RemoveAt(stack.Count - 1);
				var ctx = stack[stack.Count - 1];
				if (depth > ctx.Depth + 1)
					throw new IOException("Indentation error in configuration file line '"+line+"'");
				if (content.StartsWith(";"))
					comments.Add(content);
				else if (content.StartsWith("["))
				{
					stack.Add(new Group(ctx, content.Substring(1, content.Length - 2), comments.ToArray()));
					comments.Clear();
				}
				else
				{
					int eqAt = content.IndexOf('=');
					if (eqAt == -1)
						throw new IOException("Assignment error in configuration file line '" + line + "'");

					string name = content.Substring(0, eqAt).Trim();
					string value = content.Substring(eqAt + 1).Trim();

					string typeComment = comments[comments.Count - 1].Substring(1, comments[comments.Count - 1].Length-2);
					
					int at = comments.Count - 2;
					while (at >= 0 && !comments[at].EndsWith("."))
					{
						typeComment = comments[at].Substring(1) + typeComment;
						at--;
					}

					var parts = typeComment.Split(';');
					string range = null;
					string def = null;
					for (int i = 1; i < parts.Length; i++)
					{
						var p = parts[i].Trim();
						if (p.StartsWith("range="))
							range = p.Substring(6);
						else if (p.StartsWith("default="))
							def = p.Substring(8);
					}

					Attribute attrib;

					if (parts[0].ToLower().Contains("set"))
					{
						attrib = new SetAttribute(ctx, name, range, comments.ToArray());
					}
					else if (parts[0].ToLower().EndsWith(" enumeration"))
					{
						attrib = new EnumAttribute(ctx, name, range, comments.ToArray());
					}
					else
					{
						switch (parts[0].ToLower())
						{
							case "bool":
								attrib = new BoolAttribute(ctx, name, comments.ToArray());
								break;
							case "string":
								attrib = new StringAttribute(ctx, name, comments.ToArray());
								break;
							case "float32":
								attrib = new FloatAttribute(ctx, name, range, comments.ToArray());
								break;
							case "int32":
							case "uint32":
							case "count16":
								attrib = new IntAttribute(ctx, name, range, comments.ToArray());
								break;
							default:
								attrib = new UnsupportedAttribute(ctx, name, parts[0], comments.ToArray());
								break;
						}
					}
					attrib.StringValue = value;
					comments.Clear();

				}


			}



			foreach (Control c in attributePanel.Controls)
				Tidy(c);
		}

		private static void Tidy(Control c)
		{
			foreach (Control child in c.Controls)
				Tidy(child);

			c.Height = c.PreferredSize.Height;
		}

		private bool deleteOnExit = true;

		public void Show(string commitText, Action<string> onCompleteHandlePath)
		{
			this.acceptButton.Text = commitText;
			this.onCompleteHandlePath = onCompleteHandlePath;

			fileName = Path.GetTempFileName();


			Reload();

			this.Show();
		}

		public void Show(string existingOutFileName, string commitText, Action<string> onCompleteHandlePath)
		{
			this.acceptButton.Text = commitText;
			this.onCompleteHandlePath = onCompleteHandlePath;

			fileName = existingOutFileName;
			deleteOnExit = false;

			Reload();

			this.Show();
		}

		private void abortButton_Click(object sender, EventArgs e)
		{
			this.Close();
		}

		private void acceptButton_Click(object sender, EventArgs e)
		{
			using (var writer = new StreamWriter(fileName, false))
			{
				root.Write(writer);
			}


			onCompleteHandlePath(fileName);
			this.Close();
		}

		private void EditServerConfig_FormClosed(object sender, FormClosedEventArgs e)
		{
			if (deleteOnExit)
				File.Delete(fileName);
		}
	}
}
