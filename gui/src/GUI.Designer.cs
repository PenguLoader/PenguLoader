namespace LeagueLoader
{
    partial class GUI
    {
        /// <summary>
        /// Required designer variable.
        /// </summary>
        private System.ComponentModel.IContainer components = null;

        /// <summary>
        /// Clean up any resources being used.
        /// </summary>
        /// <param name="disposing">true if managed resources should be disposed; otherwise, false.</param>
        protected override void Dispose(bool disposing)
        {
            if (disposing && (components != null))
            {
                components.Dispose();
            }
            base.Dispose(disposing);
        }

        #region Windows Form Designer generated code

        /// <summary>
        /// Required method for Designer support - do not modify
        /// the contents of this method with the code editor.
        /// </summary>
        private void InitializeComponent()
        {
            System.ComponentModel.ComponentResourceManager resources = new System.ComponentModel.ComponentResourceManager(typeof(GUI));
            this.lblInsecureOptions = new MetroFramework.Controls.MetroLabel();
            this.chkDWS = new MetroFramework.Controls.MetroCheckBox();
            this.chkICE = new MetroFramework.Controls.MetroCheckBox();
            this.lnkRemoteDevtools = new MetroFramework.Controls.MetroLink();
            this.btnOpenDevTools = new MetroFramework.Controls.MetroButton();
            this.btnOpenPlugins = new MetroFramework.Controls.MetroButton();
            this.btnInstall = new MetroFramework.Controls.MetroButton();
            this.label3 = new System.Windows.Forms.Label();
            this.label1 = new System.Windows.Forms.Label();
            this.label2 = new System.Windows.Forms.Label();
            this.chkRDP = new MetroFramework.Controls.MetroCheckBox();
            this.txtPort = new MetroFramework.Controls.MetroTextBox();
            this.lnkLanguage = new MetroFramework.Controls.MetroLink();
            this.lblLeaguePath = new MetroFramework.Controls.MetroLabel();
            this.txtPath = new MetroFramework.Controls.MetroTextBox();
            this.btnSelectPath = new MetroFramework.Controls.MetroButton();
            this.btnRestartLC = new MetroFramework.Controls.MetroButton();
            this.lnkGithub = new MetroFramework.Controls.MetroLink();
            this.SuspendLayout();
            // 
            // lblInsecureOptions
            // 
            this.lblInsecureOptions.AutoSize = true;
            this.lblInsecureOptions.Location = new System.Drawing.Point(23, 143);
            this.lblInsecureOptions.Margin = new System.Windows.Forms.Padding(3, 5, 3, 5);
            this.lblInsecureOptions.Name = "lblInsecureOptions";
            this.lblInsecureOptions.Size = new System.Drawing.Size(103, 19);
            this.lblInsecureOptions.TabIndex = 19;
            this.lblInsecureOptions.Text = "Insecure options";
            // 
            // chkDWS
            // 
            this.chkDWS.AutoSize = true;
            this.chkDWS.Location = new System.Drawing.Point(37, 191);
            this.chkDWS.Name = "chkDWS";
            this.chkDWS.Size = new System.Drawing.Size(130, 15);
            this.chkDWS.TabIndex = 18;
            this.chkDWS.TabStop = false;
            this.chkDWS.Text = "Disable web security";
            this.chkDWS.UseSelectable = true;
            this.chkDWS.CheckedChanged += new System.EventHandler(this.chkDWS_CheckedChanged);
            // 
            // chkICE
            // 
            this.chkICE.AutoSize = true;
            this.chkICE.Location = new System.Drawing.Point(37, 170);
            this.chkICE.Name = "chkICE";
            this.chkICE.Size = new System.Drawing.Size(145, 15);
            this.chkICE.TabIndex = 17;
            this.chkICE.TabStop = false;
            this.chkICE.Text = "Ignore certificate errors";
            this.chkICE.UseSelectable = true;
            this.chkICE.CheckedChanged += new System.EventHandler(this.chkICE_CheckedChanged);
            // 
            // lnkRemoteDevtools
            // 
            this.lnkRemoteDevtools.Location = new System.Drawing.Point(161, 267);
            this.lnkRemoteDevtools.Name = "lnkRemoteDevtools";
            this.lnkRemoteDevtools.Size = new System.Drawing.Size(131, 23);
            this.lnkRemoteDevtools.TabIndex = 34;
            this.lnkRemoteDevtools.Text = "[Remote DevTools ↗]";
            this.lnkRemoteDevtools.UseSelectable = true;
            this.lnkRemoteDevtools.Click += new System.EventHandler(this.lnkRemoteDevTools_Click);
            // 
            // btnOpenDevTools
            // 
            this.btnOpenDevTools.FontWeight = MetroFramework.MetroButtonWeight.Regular;
            this.btnOpenDevTools.Location = new System.Drawing.Point(23, 267);
            this.btnOpenDevTools.Name = "btnOpenDevTools";
            this.btnOpenDevTools.Size = new System.Drawing.Size(132, 23);
            this.btnOpenDevTools.TabIndex = 33;
            this.btnOpenDevTools.Text = "Open DevTools";
            this.btnOpenDevTools.UseSelectable = true;
            this.btnOpenDevTools.Click += new System.EventHandler(this.btnOpenDevTools_Click);
            // 
            // btnOpenPlugins
            // 
            this.btnOpenPlugins.FontWeight = MetroFramework.MetroButtonWeight.Regular;
            this.btnOpenPlugins.Location = new System.Drawing.Point(23, 238);
            this.btnOpenPlugins.Name = "btnOpenPlugins";
            this.btnOpenPlugins.Size = new System.Drawing.Size(132, 23);
            this.btnOpenPlugins.TabIndex = 32;
            this.btnOpenPlugins.Text = "Open plugins folder";
            this.btnOpenPlugins.UseSelectable = true;
            this.btnOpenPlugins.Click += new System.EventHandler(this.btnPlugins_Click);
            // 
            // btnInstall
            // 
            this.btnInstall.FontSize = MetroFramework.MetroButtonSize.Tall;
            this.btnInstall.FontWeight = MetroFramework.MetroButtonWeight.Regular;
            this.btnInstall.Location = new System.Drawing.Point(359, 254);
            this.btnInstall.Name = "btnInstall";
            this.btnInstall.Size = new System.Drawing.Size(131, 36);
            this.btnInstall.TabIndex = 30;
            this.btnInstall.Text = "INSTALL";
            this.btnInstall.UseSelectable = true;
            this.btnInstall.Click += new System.EventHandler(this.btnInstall_Click);
            // 
            // label3
            // 
            this.label3.BackColor = System.Drawing.Color.White;
            this.label3.Font = new System.Drawing.Font("Segoe UI", 18F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
            this.label3.ForeColor = System.Drawing.Color.FromArgb(((int)(((byte)(64)))), ((int)(((byte)(64)))), ((int)(((byte)(64)))));
            this.label3.Location = new System.Drawing.Point(123, 16);
            this.label3.Name = "label3";
            this.label3.Size = new System.Drawing.Size(97, 39);
            this.label3.TabIndex = 37;
            this.label3.Text = "Loader";
            this.label3.TextAlign = System.Drawing.ContentAlignment.MiddleLeft;
            this.label3.MouseDown += new System.Windows.Forms.MouseEventHandler(this.MouseDownDragMove);
            // 
            // label1
            // 
            this.label1.BackColor = System.Drawing.Color.FromArgb(((int)(((byte)(0)))), ((int)(((byte)(174)))), ((int)(((byte)(219)))));
            this.label1.Image = ((System.Drawing.Image)(resources.GetObject("label1.Image")));
            this.label1.Location = new System.Drawing.Point(7, 24);
            this.label1.Name = "label1";
            this.label1.Size = new System.Drawing.Size(24, 24);
            this.label1.TabIndex = 38;
            this.label1.MouseDown += new System.Windows.Forms.MouseEventHandler(this.MouseDownDragMove);
            // 
            // label2
            // 
            this.label2.BackColor = System.Drawing.Color.FromArgb(((int)(((byte)(0)))), ((int)(((byte)(174)))), ((int)(((byte)(219)))));
            this.label2.Font = new System.Drawing.Font("Segoe UI", 18F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
            this.label2.ForeColor = System.Drawing.Color.White;
            this.label2.Location = new System.Drawing.Point(0, 16);
            this.label2.Name = "label2";
            this.label2.Size = new System.Drawing.Size(123, 39);
            this.label2.TabIndex = 36;
            this.label2.Text = "League";
            this.label2.TextAlign = System.Drawing.ContentAlignment.MiddleRight;
            this.label2.MouseDown += new System.Windows.Forms.MouseEventHandler(this.MouseDownDragMove);
            // 
            // chkRDP
            // 
            this.chkRDP.AutoSize = true;
            this.chkRDP.Location = new System.Drawing.Point(223, 170);
            this.chkRDP.Name = "chkRDP";
            this.chkRDP.Size = new System.Drawing.Size(150, 15);
            this.chkRDP.TabIndex = 24;
            this.chkRDP.TabStop = false;
            this.chkRDP.Text = "Remote debugging port";
            this.chkRDP.UseSelectable = true;
            this.chkRDP.CheckedChanged += new System.EventHandler(this.chkRDP_CheckedChanged);
            // 
            // txtPort
            // 
            // 
            // 
            // 
            this.txtPort.CustomButton.Image = null;
            this.txtPort.CustomButton.Location = new System.Drawing.Point(39, 1);
            this.txtPort.CustomButton.Name = "";
            this.txtPort.CustomButton.Size = new System.Drawing.Size(21, 21);
            this.txtPort.CustomButton.Style = MetroFramework.MetroColorStyle.Blue;
            this.txtPort.CustomButton.TabIndex = 1;
            this.txtPort.CustomButton.Theme = MetroFramework.MetroThemeStyle.Light;
            this.txtPort.CustomButton.UseSelectable = true;
            this.txtPort.CustomButton.Visible = false;
            this.txtPort.Enabled = false;
            this.txtPort.Lines = new string[] {
        "8888"};
            this.txtPort.Location = new System.Drawing.Point(379, 166);
            this.txtPort.MaxLength = 32767;
            this.txtPort.Name = "txtPort";
            this.txtPort.PasswordChar = '\0';
            this.txtPort.ScrollBars = System.Windows.Forms.ScrollBars.None;
            this.txtPort.SelectedText = "";
            this.txtPort.SelectionLength = 0;
            this.txtPort.SelectionStart = 0;
            this.txtPort.ShortcutsEnabled = true;
            this.txtPort.Size = new System.Drawing.Size(61, 23);
            this.txtPort.TabIndex = 41;
            this.txtPort.Text = "8888";
            this.txtPort.TextAlign = System.Windows.Forms.HorizontalAlignment.Center;
            this.txtPort.UseSelectable = true;
            this.txtPort.WaterMarkColor = System.Drawing.Color.FromArgb(((int)(((byte)(109)))), ((int)(((byte)(109)))), ((int)(((byte)(109)))));
            this.txtPort.WaterMarkFont = new System.Drawing.Font("Segoe UI", 12F, System.Drawing.FontStyle.Italic, System.Drawing.GraphicsUnit.Pixel);
            this.txtPort.TextChanged += new System.EventHandler(this.txtPort_TextChanged);
            this.txtPort.KeyPress += new System.Windows.Forms.KeyPressEventHandler(this.txtPort_KeyPress);
            // 
            // lnkLanguage
            // 
            this.lnkLanguage.AutoSize = true;
            this.lnkLanguage.Location = new System.Drawing.Point(330, 32);
            this.lnkLanguage.Name = "lnkLanguage";
            this.lnkLanguage.Size = new System.Drawing.Size(89, 23);
            this.lnkLanguage.TabIndex = 42;
            this.lnkLanguage.Text = "[Tiếng Việt]";
            this.lnkLanguage.TextAlign = System.Drawing.ContentAlignment.MiddleRight;
            this.lnkLanguage.UseSelectable = true;
            this.lnkLanguage.Click += new System.EventHandler(this.lnkLanguage_Click);
            // 
            // lblLeaguePath
            // 
            this.lblLeaguePath.AutoSize = true;
            this.lblLeaguePath.Location = new System.Drawing.Point(23, 78);
            this.lblLeaguePath.Margin = new System.Windows.Forms.Padding(3, 5, 3, 5);
            this.lblLeaguePath.Name = "lblLeaguePath";
            this.lblLeaguePath.Size = new System.Drawing.Size(118, 19);
            this.lblLeaguePath.TabIndex = 20;
            this.lblLeaguePath.Text = "League Client path";
            // 
            // txtPath
            // 
            // 
            // 
            // 
            this.txtPath.CustomButton.Image = null;
            this.txtPath.CustomButton.Location = new System.Drawing.Point(381, 1);
            this.txtPath.CustomButton.Name = "";
            this.txtPath.CustomButton.Size = new System.Drawing.Size(21, 21);
            this.txtPath.CustomButton.Style = MetroFramework.MetroColorStyle.Blue;
            this.txtPath.CustomButton.TabIndex = 1;
            this.txtPath.CustomButton.Theme = MetroFramework.MetroThemeStyle.Light;
            this.txtPath.CustomButton.UseSelectable = true;
            this.txtPath.CustomButton.Visible = false;
            this.txtPath.Enabled = false;
            this.txtPath.Lines = new string[] {
        "[not found]"};
            this.txtPath.Location = new System.Drawing.Point(37, 105);
            this.txtPath.MaxLength = 32767;
            this.txtPath.Name = "txtPath";
            this.txtPath.PasswordChar = '\0';
            this.txtPath.ReadOnly = true;
            this.txtPath.ScrollBars = System.Windows.Forms.ScrollBars.None;
            this.txtPath.SelectedText = "";
            this.txtPath.SelectionLength = 0;
            this.txtPath.SelectionStart = 0;
            this.txtPath.ShortcutsEnabled = true;
            this.txtPath.Size = new System.Drawing.Size(403, 23);
            this.txtPath.TabIndex = 21;
            this.txtPath.Text = "[not found]";
            this.txtPath.UseSelectable = true;
            this.txtPath.WaterMarkColor = System.Drawing.Color.FromArgb(((int)(((byte)(109)))), ((int)(((byte)(109)))), ((int)(((byte)(109)))));
            this.txtPath.WaterMarkFont = new System.Drawing.Font("Segoe UI", 12F, System.Drawing.FontStyle.Italic, System.Drawing.GraphicsUnit.Pixel);
            // 
            // btnSelectPath
            // 
            this.btnSelectPath.FontWeight = MetroFramework.MetroButtonWeight.Regular;
            this.btnSelectPath.Location = new System.Drawing.Point(446, 105);
            this.btnSelectPath.Name = "btnSelectPath";
            this.btnSelectPath.Size = new System.Drawing.Size(44, 23);
            this.btnSelectPath.TabIndex = 43;
            this.btnSelectPath.Text = "...";
            this.btnSelectPath.UseSelectable = true;
            this.btnSelectPath.Click += new System.EventHandler(this.btnSelectPath_Click);
            // 
            // btnRestartLC
            // 
            this.btnRestartLC.FontWeight = MetroFramework.MetroButtonWeight.Regular;
            this.btnRestartLC.Location = new System.Drawing.Point(161, 238);
            this.btnRestartLC.Name = "btnRestartLC";
            this.btnRestartLC.Size = new System.Drawing.Size(131, 23);
            this.btnRestartLC.TabIndex = 44;
            this.btnRestartLC.Text = "Restart Client";
            this.btnRestartLC.UseSelectable = true;
            this.btnRestartLC.Click += new System.EventHandler(this.btnRestartLC_Click);
            // 
            // lnkGithub
            // 
            this.lnkGithub.Location = new System.Drawing.Point(419, 32);
            this.lnkGithub.Name = "lnkGithub";
            this.lnkGithub.Size = new System.Drawing.Size(71, 23);
            this.lnkGithub.TabIndex = 45;
            this.lnkGithub.Text = "[Github ↗]";
            this.lnkGithub.UseSelectable = true;
            this.lnkGithub.Click += new System.EventHandler(this.lnkGithub_Click);
            // 
            // GUI
            // 
            this.AutoScaleDimensions = new System.Drawing.SizeF(6F, 13F);
            this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
            this.ClientSize = new System.Drawing.Size(513, 317);
            this.Controls.Add(this.lnkGithub);
            this.Controls.Add(this.btnRestartLC);
            this.Controls.Add(this.btnSelectPath);
            this.Controls.Add(this.lnkLanguage);
            this.Controls.Add(this.txtPort);
            this.Controls.Add(this.chkRDP);
            this.Controls.Add(this.label1);
            this.Controls.Add(this.label2);
            this.Controls.Add(this.lblLeaguePath);
            this.Controls.Add(this.lnkRemoteDevtools);
            this.Controls.Add(this.btnOpenDevTools);
            this.Controls.Add(this.chkICE);
            this.Controls.Add(this.btnOpenPlugins);
            this.Controls.Add(this.txtPath);
            this.Controls.Add(this.chkDWS);
            this.Controls.Add(this.btnInstall);
            this.Controls.Add(this.lblInsecureOptions);
            this.Controls.Add(this.label3);
            this.Icon = ((System.Drawing.Icon)(resources.GetObject("$this.Icon")));
            this.MaximizeBox = false;
            this.Name = "GUI";
            this.Resizable = false;
            this.ShadowType = MetroFramework.Forms.MetroFormShadowType.AeroShadow;
            this.Text = "League Loader";
            this.Load += new System.EventHandler(this.GUI_Load);
            this.ResumeLayout(false);
            this.PerformLayout();

        }

        #endregion
        private MetroFramework.Controls.MetroLabel lblInsecureOptions;
        private MetroFramework.Controls.MetroCheckBox chkDWS;
        private MetroFramework.Controls.MetroCheckBox chkICE;
        private MetroFramework.Controls.MetroLink lnkRemoteDevtools;
        private MetroFramework.Controls.MetroButton btnOpenDevTools;
        private MetroFramework.Controls.MetroButton btnOpenPlugins;
        private MetroFramework.Controls.MetroButton btnInstall;
        private System.Windows.Forms.Label label3;
        private System.Windows.Forms.Label label1;
        private System.Windows.Forms.Label label2;
        private MetroFramework.Controls.MetroCheckBox chkRDP;
        private MetroFramework.Controls.MetroTextBox txtPort;
        private MetroFramework.Controls.MetroLink lnkLanguage;
        private MetroFramework.Controls.MetroLabel lblLeaguePath;
        private MetroFramework.Controls.MetroTextBox txtPath;
        private MetroFramework.Controls.MetroButton btnSelectPath;
        private MetroFramework.Controls.MetroButton btnRestartLC;
        private MetroFramework.Controls.MetroLink lnkGithub;
    }
}

