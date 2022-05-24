using System;
using System.Threading.Tasks;
using System.Windows.Forms;
using System.Diagnostics;
using System.IO;
using MetroFramework.Forms;
using System.Runtime.InteropServices;

namespace LeagueLoader
{
    public partial class GUI : MetroForm
    {
        private const string ENV_DIR = "LEAGUE_LOADER_DIR";

        private string PluginsDir => Directory.GetCurrentDirectory() + @"\plugins";

        private Ini _config = null;
        private int _port = 0;
        private string _lcPath = "";
        private bool _installed = false;
        private bool _lcRunning = false;

        public GUI()
        {
            InitializeComponent();
        }

        private void GUI_Load(object sender, EventArgs e)
        {
            if (!Directory.Exists(PluginsDir))
            {
                Directory.CreateDirectory(PluginsDir);
            }

            LoadConfig();

            Invoke(new Action(async () =>
            {
                while (true)
                {
                    var procs = Process.GetProcessesByName("LeagueClientUx");
                    _lcRunning = procs.Length > 0;
                    btnOpenDevTools.Enabled = _lcRunning && _installed;
                    linkRemote.Enabled = _lcRunning && _installed;
                    panelMain.Enabled = !_lcRunning;

                    if (_lcRunning && string.IsNullOrEmpty(_lcPath))
                    {
                        _lcPath = Path.GetDirectoryName(procs[0].MainModule.FileName);
                        textPath.Text = _lcPath;
                        _config.Write("GENERAL", "LeagueClientPath", _lcPath);
                    }

                    btnInstall.Enabled = !_lcRunning;
                    await Task.Delay(300);
                }
            }));
        }

        private void LoadConfig()
        {
            var configPath = Directory.GetCurrentDirectory() + @"\config.cfg";
            if (!File.Exists(configPath)) File.CreateText(configPath);
            var config = new Ini(configPath);

            var port = config.Read("GENERAL", "RemoteDebuggingPort");
            checkICE.Checked = config.Read("GENERAL", "IgnoreCertificateErrors") == "1";
            checkDWS.Checked = config.Read("GENERAL", "DisableWebSecurity") == "1";
            var path = config.Read("GENERAL", "LeagueClientPath");

            if (File.Exists(path + @"\LeagueClient.exe"))
            {
                _lcPath = path;
                textPath.Text = path;
            }

            int.TryParse(port, out _port);
            radioPort.Checked = _port != 0;
            textPort.Enabled = _port != 0;
            linkRemote.Visible = _port != 0;
            textPort.Text = (_port == 0 ? 8888 : _port).ToString();

            _installed = !string.IsNullOrEmpty(Environment.GetEnvironmentVariable(ENV_DIR))
            && !string.IsNullOrEmpty(_lcPath) && File.Exists(_lcPath + @"\d3d9.dll");
            btnInstall.Text = _installed ? "Uninstall" : "Install";

            _config = config;
        }

        private void btnSelectPath_Click(object sender, EventArgs e)
        {
            using (var fbd = new FolderBrowserDialog())
            {
                fbd.Description = "Select LoL folder or LeagueClient folder";
                DialogResult result = fbd.ShowDialog();

                if (result == DialogResult.OK && !string.IsNullOrWhiteSpace(fbd.SelectedPath))
                {
                    if (File.Exists(fbd.SelectedPath + @"\LeagueClient.exe"))
                    {
                        _lcPath = fbd.SelectedPath;
                    }
                    else if (File.Exists(fbd.SelectedPath + @"\LeagueClient\LeagueClient.exe"))
                    {
                        _lcPath = fbd.SelectedPath + @"\LeagueClient";
                    }
                    else
                    {
                        MessageBox.Show(this, "Your selected path is invalid.",
                            "League Loader", MessageBoxButtons.OK, MessageBoxIcon.Warning);
                        return;
                    }

                    _config.Write("GENERAL", "LeagueClientPath", _lcPath);
                    textPath.Text = _lcPath;
                }
            }
        }

        private void btnPlugins_Click(object sender, EventArgs e)
        {
            if (!Directory.Exists(PluginsDir))
            {
                Directory.CreateDirectory(PluginsDir);
            }

            Process.Start(new ProcessStartInfo()
            {
                FileName = "explorer.exe",
                Arguments = $"\"{PluginsDir}\"",
                UseShellExecute = true
            });
        }

        private void UpdatePort(int port)
        {
            if (_config == null) return;

            _port = port;
            _config.Write("GENERAL", "RemoteDebuggingPort", port.ToString());
        }

        private void radioOff_CheckedChanged(object sender, EventArgs e)
        {
            if (_config == null) return;

            if (radioOff.Checked)
            {
                textPort.Enabled = false;
                linkRemote.Visible = false;
                UpdatePort(0);
            }
        }

        private void radioPort_CheckedChanged(object sender, EventArgs e)
        {
            if (_config == null) return;

            if (radioPort.Checked)
            {
                MessageBox.Show(this,
                    "Using remote debugging port enables remote debug over HTTP on the specified port." +
                    " You can access the remote DevTools in Chromium browsers and use DevTools protocol via this port." +
                    " That will modify League Client's arguments, and be recorded in the logs.",
                    "League Loader", MessageBoxButtons.OK, MessageBoxIcon.Warning);

                textPort.Enabled = true;
                linkRemote.Visible = true;
                int port = 0;
                int.TryParse(textPort.Text, out port);
                UpdatePort(port);
            }
        }

        private void btnOpenDevTools_Click(object sender, EventArgs e)
        {
            if (_installed && _lcRunning)
            {
                Dll.Open(false);
            }
            else
            {
                MessageBox.Show(this, "Please make sure you've installed and League Client is opened.",
                    "League Loader", MessageBoxButtons.OK, MessageBoxIcon.Warning);
            }
        }

        private void linkRemote_Click(object sender, EventArgs e)
        {
            if (_installed && _lcRunning)
            {
                Dll.Open(true);
            }
            else
            {
                MessageBox.Show(this, "Please make sure you've installed and League Client is opened.",
                    "League Loader", MessageBoxButtons.OK, MessageBoxIcon.Warning);
            }
        }

        private void btnInstall_Click(object sender, EventArgs e)
        {
            if (_lcRunning)
            {
                MessageBox.Show(this, "Please make sure League Client is closed.",
                    "League Loader", MessageBoxButtons.OK, MessageBoxIcon.Warning);
                return;
            }

            if (string.IsNullOrEmpty(_lcPath))
            {
                MessageBox.Show(this, "Please select League Client path.",
                   "League Loader", MessageBoxButtons.OK, MessageBoxIcon.Warning);
                btnSelectPath_Click(null, null);
            }

            if (_installed)
            {
                Environment.SetEnvironmentVariable(ENV_DIR, null, EnvironmentVariableTarget.User);
                File.Delete(_lcPath + @"\d3d9.dll");

                _installed = false;
                btnInstall.Text = "Install";
                MessageBox.Show(this, "Uninstalled successfully.",
                    "League Loader", MessageBoxButtons.OK, MessageBoxIcon.Information);
            }
            else
            {
                var cd = Directory.GetCurrentDirectory();
                var d3d9 = cd + @"\d3d9.dll";

                if (!File.Exists(d3d9))
                {
                    MessageBox.Show(this, "League Loader d3d9.dll is not found.",
                        "League Loader", MessageBoxButtons.OK, MessageBoxIcon.Warning);
                    return;
                }

                Environment.SetEnvironmentVariable(ENV_DIR, cd, EnvironmentVariableTarget.User);

                try
                {
                    File.Copy(cd + @"\d3d9.dll", _lcPath + @"\d3d9.dll", true);
                }
                catch
                {
                    MessageBox.Show(this, "Failed to copy d3d9.dll.",
                        "League Loader", MessageBoxButtons.OK, MessageBoxIcon.Warning);
                    return;
                }

                _installed = true;
                btnInstall.Text = "Uninstall";
                MessageBox.Show(this, "Installed successfully.",
                    "League Loader", MessageBoxButtons.OK, MessageBoxIcon.Information);
            }
        }

        private void checkICE_CheckedChanged(object sender, EventArgs e)
        {
            if (_config == null) return;

            if (checkICE.Checked)
            {
                MessageBox.Show(this,
                    "Ignoring certificate errors helps you to ignore all SSL errors/invalid certificates." +
                    " That's against Riot's Privacy Policy, so you might get banned.",
                    "League Loader", MessageBoxButtons.OK, MessageBoxIcon.Warning);
            }

            _config.Write("GENERAL", "IgnoreCertificateErrors", checkICE.Checked ? "1" : "0");
        }

        private void checkDWS_CheckedChanged(object sender, EventArgs e)
        {
            if (_config == null) return;

            if (checkDWS.Checked)
            {
                MessageBox.Show(this,
                    "Disabling web security helps you to bypass CORS when making request with fetch() and XHR." +
                    " That's against Riot's Privacy Policy, so you might get banned.",
                    "League Loader", MessageBoxButtons.OK, MessageBoxIcon.Warning);
            }

            _config.Write("GENERAL", "DisableWebSecurity", checkDWS.Checked ? "1" : "0");
        }

        private void linkRepo_LinkClicked(object sender, LinkLabelLinkClickedEventArgs e)
        {
            Process.Start("https://github.com/nomi-san/league-loader");
        }

        private void textPort_TextChanged(object sender, EventArgs e)
        {
            if (_config == null) return;

            if (radioPort.Checked)
            {
                int port = 0;
                int.TryParse(textPort.Text, out port);
                UpdatePort(port);
            }
        }

        private void textPort_KeyPress(object sender, KeyPressEventArgs e)
        {
            e.Handled = !char.IsDigit(e.KeyChar) && !char.IsControl(e.KeyChar);
        }

        private const int WM_NCLBUTTONDOWN = 0xA1;
        private const int HT_CAPTION = 0x2;

        [DllImport("user32.dll")]
        private static extern IntPtr SendMessage(IntPtr hWnd, int Msg, IntPtr wParam, IntPtr lParam);
        [DllImport("user32.dll")]
        private static extern bool ReleaseCapture();

        private void MouseDownDragMove(object sender, MouseEventArgs e)
        {
            if (e.Button == MouseButtons.Left)
            {
                ReleaseCapture();
                SendMessage(Handle, WM_NCLBUTTONDOWN, (IntPtr)HT_CAPTION, IntPtr.Zero);
            }
        }
    }
}
