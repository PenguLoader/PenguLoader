using System;
using System.Threading.Tasks;
using System.Windows.Forms;
using System.Diagnostics;
using System.IO;
using MetroFramework.Forms;
using System.Runtime.InteropServices;

namespace LeagueLoader
{
    internal partial class GUI : MetroForm
    {
        public static GUI Instance { get; private set; } = null;

        static string PluginsDir = Path.Combine(Directory.GetCurrentDirectory(), "plugins");

        public GUI()
        {
            Instance = this;
            InitializeComponent();

            Config.Init();

            var port = Config.RemoteDebuggingPort;
            chkRDP.Checked = port > 0;
            txtPort.Enabled = port > 0;
            txtPort.Text = port > 0 ? port.ToString() : "8888";

            lblVersion.Text = $"v{Updater.CurrentVersion.ToString(3)}";
        }

        private void GUI_Load(object sender, EventArgs e)
        {
            if (!Directory.Exists(PluginsDir))
                Directory.CreateDirectory(PluginsDir);

            if (Lcu.IsValidDir(Config.LeaguePath))
            {
                SetLeaguePath(Config.LeaguePath);
            }
            else if (Lcu.IsOpened())
            {
                SetLeaguePath(Lcu.GetDir());
            }
            else
            {
                Config.LeaguePath = "";
                txtPath.Text = "[not found]";
            }
        }

        [DllImport("user32.dll")]
        private static extern IntPtr SendMessage(IntPtr hWnd, int Msg, IntPtr wParam, IntPtr lParam);
        [DllImport("user32.dll")]
        private static extern bool ReleaseCapture();

        private void MouseDownDragMove(object sender, MouseEventArgs e)
        {
            if (e.Button == MouseButtons.Left)
            {
                ReleaseCapture();
                SendMessage(Handle, 0xA1, (IntPtr)0x2, (IntPtr)0);
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

        private void btnOpenDevTools_Click(object sender, EventArgs e)
        {
            if (Dll.IsLoaded())
            {
                Dll.OpenDevTools(remote: false);
            }
            else
            {
                MessageBox.Show(this, _l.Msg_NotActivated,
                    "League Loader", MessageBoxButtons.OK, MessageBoxIcon.Warning);
            }
        }

        private void lnkRemoteDevTools_Click(object sender, EventArgs e)
        {
            if (Dll.IsLoaded())
            {
                Dll.OpenDevTools(remote: true);
            }
            else
            {
                MessageBox.Show(this, _l.Msg_NotActivated,
                    "League Loader", MessageBoxButtons.OK, MessageBoxIcon.Warning);
            }
        }

        private void btnRestartLC_Click(object sender, EventArgs e)
        {
            if (Lcu.IsOpened())
            {
                Lcu.KillUxAndRestart();
            }
            else
            {
                MessageBox.Show(this, _l.Msg_LeagueNotOpened,
                    "League Loader", MessageBoxButtons.OK, MessageBoxIcon.Warning);
            }
        }

        private void btnInstall_Click(object sender, EventArgs e)
        {
            var lcPath = txtPath.Text;

            if (Lcu.IsValidDir(lcPath))
            {
                if (Dll.IsInstalled(lcPath))
                {
                    if (MessageBox.Show(this, _l.Msg_ModuleUninstall,
                            "League Loader", MessageBoxButtons.YesNo, MessageBoxIcon.Information) == DialogResult.Yes)
                    {
                        Dll.Uninstall(lcPath);
                        btnInstall.Text = _l.Install;

                        if (Dll.IsLoaded())
                        {
                            if (MessageBox.Show(this, _l.Msg_ModuleUninstalled_Loaded,
                                "League Loader", MessageBoxButtons.YesNo, MessageBoxIcon.Information) == DialogResult.Yes)
                            {
                                Lcu.KillUxAndRestart();
                            }
                        }
                        else
                        {
                            MessageBox.Show(this, _l.Msg_ModuleUninstalled,
                                "Legue Loader", MessageBoxButtons.OK, MessageBoxIcon.Information);
                        }
                    }
                }
                else
                {
                    Dll.Install(lcPath);
                    btnInstall.Text = _l.Uninstall;

                    if (Lcu.IsOpened())
                    {
                        if (MessageBox.Show(this, _l.Msg_ModuleInstalled_Running,
                            "League Loader", MessageBoxButtons.YesNo, MessageBoxIcon.Information) == DialogResult.Yes)
                        {
                            Lcu.KillUxAndRestart();
                        }
                    }
                    else
                    {
                        MessageBox.Show(this, _l.Msg_ModuleInstalled,
                            "League Loader", MessageBoxButtons.OK, MessageBoxIcon.Information);
                    }
                }
            }
            else
            {
                MessageBox.Show(this, _l.Msg_InvalidSelectedPath,
                    "League Loader", MessageBoxButtons.OK, MessageBoxIcon.Warning);
            }
        }

        private void chkRDP_CheckedChanged(object sender, EventArgs e)
        {
            if (!Visible) return;

            if (chkRDP.Checked)
            {
                var ret = MessageBox.Show(this, _l.WarningRemoteDebugger,
                    "League Loader", MessageBoxButtons.YesNo, MessageBoxIcon.Warning);

                if (ret != DialogResult.Yes) chkRDP.Checked = false;
            }

            txtPort.Enabled = chkRDP.Checked;

            int port = 0;
            if (chkRDP.Checked) int.TryParse(txtPort.Text, out port);
            Config.RemoteDebuggingPort = port;
        }

        private void txtPort_TextChanged(object sender, EventArgs e)
        {
            if (!Visible) return;

            if (chkRDP.Checked)
            {
                int port = 0;
                int.TryParse(txtPort.Text, out port);
                Config.RemoteDebuggingPort = port;
            }
        }

        private void txtPort_KeyPress(object sender, KeyPressEventArgs e)
        {
            e.Handled = !char.IsDigit(e.KeyChar) && !char.IsControl(e.KeyChar);
        }

        Language _l = Language.English;

        void SwitchLanguage()
        {
            if (_l == Language.English)
            {
                lnkLanguage.Text = "[English]";
                _l = Language.Vietnamese;
            }
            else
            {
                lnkLanguage.Text = "[Tiếng Việt]";
                _l = Language.English;
            }

            lblLeaguePath.Text = _l.LeaguePath;
            lblRemoteDebugger.Text = _l.RemoteDebugger;
            chkRDP.Text = _l.EnableWithPort;
            btnOpenDevTools.Text = _l.OpenDevTools;
            btnOpenPlugins.Text = _l.OpenPlugins;
            btnRestartLC.Text = _l.RestartClient;

            btnInstall.Text = Dll.IsInstalled(Config.LeaguePath) ? _l.Uninstall : _l.Install;
        }

        private void lnkLanguage_Click(object sender, EventArgs e)
        {
            SwitchLanguage();
        }

        private void btnSelectPath_Click(object sender, EventArgs e)
        {
            using (var fbd = new FolderBrowserDialog())
            {
                fbd.Description = _l.Msg_SelectLeaguePath;
                if (fbd.ShowDialog() == DialogResult.OK && !string.IsNullOrWhiteSpace(fbd.SelectedPath))
                {
                    var path = fbd.SelectedPath;

                    if (Lcu.IsValidDir(path)) { }
                    else if (Lcu.IsValidDir(path = Path.Combine(fbd.SelectedPath, "LeagueClient"))) { }
                    else if (Lcu.IsValidDir(path = Path.Combine(fbd.SelectedPath, "League of Legends"))) { }
                    else
                    {
                        MessageBox.Show(this, _l.Msg_InvalidSelectedPath,
                            "League Loader", MessageBoxButtons.OK, MessageBoxIcon.Warning);
                        return;
                    }

                    SetLeaguePath(path);
                }
            }
        }

        void SetLeaguePath(string path)
        {
            txtPath.Text = path;
            Config.LeaguePath = path;
            btnInstall.Text = Dll.IsInstalled(path) ? _l.Uninstall : _l.Install;
        }

        private void lnkGithub_Click(object sender, EventArgs e)
        {
            Process.Start("https://github.com/nomi-san/league-loader/");
        }
    }
}