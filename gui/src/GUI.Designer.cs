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
            this.btnSelectPath = new MetroFramework.Controls.MetroButton();
            this.textPath = new MetroFramework.Controls.MetroTextBox();
            this.metroLabel3 = new MetroFramework.Controls.MetroLabel();
            this.metroLabel2 = new MetroFramework.Controls.MetroLabel();
            this.checkDWS = new MetroFramework.Controls.MetroCheckBox();
            this.checkICE = new MetroFramework.Controls.MetroCheckBox();
            this.textPort = new MetroFramework.Controls.MetroTextBox();
            this.metroLabel1 = new MetroFramework.Controls.MetroLabel();
            this.radioOff = new MetroFramework.Controls.MetroRadioButton();
            this.radioPort = new MetroFramework.Controls.MetroRadioButton();
            this.panelMain = new MetroFramework.Controls.MetroPanel();
            this.linkRemote = new MetroFramework.Controls.MetroLink();
            this.btnOpenDevTools = new MetroFramework.Controls.MetroButton();
            this.btnPlugins = new MetroFramework.Controls.MetroButton();
            this.btnInstall = new MetroFramework.Controls.MetroButton();
            this.linkRepo = new System.Windows.Forms.LinkLabel();
            this.label3 = new System.Windows.Forms.Label();
            this.label1 = new System.Windows.Forms.Label();
            this.label2 = new System.Windows.Forms.Label();
            this.metroPanel1 = new MetroFramework.Controls.MetroPanel();
            this.panelMain.SuspendLayout();
            this.SuspendLayout();
            // 
            // btnSelectPath
            // 
            this.btnSelectPath.Location = new System.Drawing.Point(415, 33);
            this.btnSelectPath.Name = "btnSelectPath";
            this.btnSelectPath.Size = new System.Drawing.Size(34, 23);
            this.btnSelectPath.TabIndex = 22;
            this.btnSelectPath.Text = "...";
            this.btnSelectPath.UseSelectable = true;
            this.btnSelectPath.Click += new System.EventHandler(this.btnSelectPath_Click);
            // 
            // textPath
            // 
            // 
            // 
            // 
            this.textPath.CustomButton.Image = null;
            this.textPath.CustomButton.Location = new System.Drawing.Point(360, 1);
            this.textPath.CustomButton.Name = "";
            this.textPath.CustomButton.Size = new System.Drawing.Size(21, 21);
            this.textPath.CustomButton.Style = MetroFramework.MetroColorStyle.Blue;
            this.textPath.CustomButton.TabIndex = 1;
            this.textPath.CustomButton.Theme = MetroFramework.MetroThemeStyle.Light;
            this.textPath.CustomButton.UseSelectable = true;
            this.textPath.CustomButton.Visible = false;
            this.textPath.Enabled = false;
            this.textPath.Lines = new string[] {
        "Not selected"};
            this.textPath.Location = new System.Drawing.Point(27, 33);
            this.textPath.MaxLength = 32767;
            this.textPath.Name = "textPath";
            this.textPath.PasswordChar = '\0';
            this.textPath.ReadOnly = true;
            this.textPath.ScrollBars = System.Windows.Forms.ScrollBars.None;
            this.textPath.SelectedText = "";
            this.textPath.SelectionLength = 0;
            this.textPath.SelectionStart = 0;
            this.textPath.ShortcutsEnabled = true;
            this.textPath.Size = new System.Drawing.Size(382, 23);
            this.textPath.TabIndex = 21;
            this.textPath.Text = "Not selected";
            this.textPath.UseSelectable = true;
            this.textPath.WaterMarkColor = System.Drawing.Color.FromArgb(((int)(((byte)(109)))), ((int)(((byte)(109)))), ((int)(((byte)(109)))));
            this.textPath.WaterMarkFont = new System.Drawing.Font("Segoe UI", 12F, System.Drawing.FontStyle.Italic, System.Drawing.GraphicsUnit.Pixel);
            // 
            // metroLabel3
            // 
            this.metroLabel3.AutoSize = true;
            this.metroLabel3.Location = new System.Drawing.Point(16, 11);
            this.metroLabel3.Name = "metroLabel3";
            this.metroLabel3.Size = new System.Drawing.Size(118, 19);
            this.metroLabel3.TabIndex = 20;
            this.metroLabel3.Text = "League Client path";
            // 
            // metroLabel2
            // 
            this.metroLabel2.AutoSize = true;
            this.metroLabel2.Location = new System.Drawing.Point(237, 69);
            this.metroLabel2.Name = "metroLabel2";
            this.metroLabel2.Size = new System.Drawing.Size(103, 19);
            this.metroLabel2.TabIndex = 19;
            this.metroLabel2.Text = "Insecure options";
            // 
            // checkDWS
            // 
            this.checkDWS.AutoSize = true;
            this.checkDWS.Location = new System.Drawing.Point(251, 113);
            this.checkDWS.Name = "checkDWS";
            this.checkDWS.Size = new System.Drawing.Size(130, 15);
            this.checkDWS.TabIndex = 18;
            this.checkDWS.TabStop = false;
            this.checkDWS.Text = "Disable web security";
            this.checkDWS.UseSelectable = true;
            this.checkDWS.CheckedChanged += new System.EventHandler(this.checkDWS_CheckedChanged);
            // 
            // checkICE
            // 
            this.checkICE.AutoSize = true;
            this.checkICE.Location = new System.Drawing.Point(251, 91);
            this.checkICE.Name = "checkICE";
            this.checkICE.Size = new System.Drawing.Size(145, 15);
            this.checkICE.TabIndex = 17;
            this.checkICE.TabStop = false;
            this.checkICE.Text = "Ignore certificate errors";
            this.checkICE.UseSelectable = true;
            this.checkICE.CheckedChanged += new System.EventHandler(this.checkICE_CheckedChanged);
            // 
            // textPort
            // 
            // 
            // 
            // 
            this.textPort.CustomButton.Image = null;
            this.textPort.CustomButton.Location = new System.Drawing.Point(22, 1);
            this.textPort.CustomButton.Name = "";
            this.textPort.CustomButton.Size = new System.Drawing.Size(21, 21);
            this.textPort.CustomButton.Style = MetroFramework.MetroColorStyle.Blue;
            this.textPort.CustomButton.TabIndex = 1;
            this.textPort.CustomButton.Theme = MetroFramework.MetroThemeStyle.Light;
            this.textPort.CustomButton.UseSelectable = true;
            this.textPort.CustomButton.Visible = false;
            this.textPort.Enabled = false;
            this.textPort.Lines = new string[] {
        "8888"};
            this.textPort.Location = new System.Drawing.Point(47, 110);
            this.textPort.MaxLength = 5;
            this.textPort.Name = "textPort";
            this.textPort.PasswordChar = '\0';
            this.textPort.ScrollBars = System.Windows.Forms.ScrollBars.None;
            this.textPort.SelectedText = "";
            this.textPort.SelectionLength = 0;
            this.textPort.SelectionStart = 0;
            this.textPort.ShortcutsEnabled = true;
            this.textPort.Size = new System.Drawing.Size(44, 23);
            this.textPort.TabIndex = 16;
            this.textPort.TabStop = false;
            this.textPort.Text = "8888";
            this.textPort.UseSelectable = true;
            this.textPort.WaterMarkColor = System.Drawing.Color.FromArgb(((int)(((byte)(109)))), ((int)(((byte)(109)))), ((int)(((byte)(109)))));
            this.textPort.WaterMarkFont = new System.Drawing.Font("Segoe UI", 12F, System.Drawing.FontStyle.Italic, System.Drawing.GraphicsUnit.Pixel);
            this.textPort.TextChanged += new System.EventHandler(this.textPort_TextChanged);
            this.textPort.KeyPress += new System.Windows.Forms.KeyPressEventHandler(this.textPort_KeyPress);
            // 
            // metroLabel1
            // 
            this.metroLabel1.AutoSize = true;
            this.metroLabel1.Location = new System.Drawing.Point(16, 69);
            this.metroLabel1.Name = "metroLabel1";
            this.metroLabel1.Size = new System.Drawing.Size(153, 19);
            this.metroLabel1.TabIndex = 15;
            this.metroLabel1.Text = "Remote debugging port";
            // 
            // radioOff
            // 
            this.radioOff.AutoSize = true;
            this.radioOff.Checked = true;
            this.radioOff.Location = new System.Drawing.Point(27, 91);
            this.radioOff.Name = "radioOff";
            this.radioOff.Size = new System.Drawing.Size(40, 15);
            this.radioOff.TabIndex = 14;
            this.radioOff.TabStop = true;
            this.radioOff.Text = "Off";
            this.radioOff.UseSelectable = true;
            this.radioOff.CheckedChanged += new System.EventHandler(this.radioOff_CheckedChanged);
            // 
            // radioPort
            // 
            this.radioPort.AutoSize = true;
            this.radioPort.Location = new System.Drawing.Point(27, 113);
            this.radioPort.Name = "radioPort";
            this.radioPort.Size = new System.Drawing.Size(26, 15);
            this.radioPort.TabIndex = 23;
            this.radioPort.Text = " ";
            this.radioPort.UseSelectable = true;
            this.radioPort.CheckedChanged += new System.EventHandler(this.radioPort_CheckedChanged);
            // 
            // panelMain
            // 
            this.panelMain.Controls.Add(this.textPort);
            this.panelMain.Controls.Add(this.metroLabel3);
            this.panelMain.Controls.Add(this.radioPort);
            this.panelMain.Controls.Add(this.radioOff);
            this.panelMain.Controls.Add(this.metroLabel1);
            this.panelMain.Controls.Add(this.btnSelectPath);
            this.panelMain.Controls.Add(this.checkICE);
            this.panelMain.Controls.Add(this.textPath);
            this.panelMain.Controls.Add(this.checkDWS);
            this.panelMain.Controls.Add(this.metroLabel2);
            this.panelMain.HorizontalScrollbarBarColor = true;
            this.panelMain.HorizontalScrollbarHighlightOnWheel = false;
            this.panelMain.HorizontalScrollbarSize = 10;
            this.panelMain.Location = new System.Drawing.Point(6, 63);
            this.panelMain.Name = "panelMain";
            this.panelMain.Size = new System.Drawing.Size(469, 150);
            this.panelMain.TabIndex = 29;
            this.panelMain.VerticalScrollbarBarColor = true;
            this.panelMain.VerticalScrollbarHighlightOnWheel = false;
            this.panelMain.VerticalScrollbarSize = 10;
            // 
            // linkRemote
            // 
            this.linkRemote.Location = new System.Drawing.Point(129, 249);
            this.linkRemote.Name = "linkRemote";
            this.linkRemote.Size = new System.Drawing.Size(119, 23);
            this.linkRemote.TabIndex = 34;
            this.linkRemote.Text = "Remote DevTools";
            this.linkRemote.UseSelectable = true;
            this.linkRemote.Click += new System.EventHandler(this.linkRemote_Click);
            // 
            // btnOpenDevTools
            // 
            this.btnOpenDevTools.FontWeight = MetroFramework.MetroButtonWeight.Regular;
            this.btnOpenDevTools.Location = new System.Drawing.Point(23, 249);
            this.btnOpenDevTools.Name = "btnOpenDevTools";
            this.btnOpenDevTools.Size = new System.Drawing.Size(100, 23);
            this.btnOpenDevTools.TabIndex = 33;
            this.btnOpenDevTools.Text = "Open DevTools";
            this.btnOpenDevTools.UseSelectable = true;
            this.btnOpenDevTools.Click += new System.EventHandler(this.btnOpenDevTools_Click);
            // 
            // btnPlugins
            // 
            this.btnPlugins.FontWeight = MetroFramework.MetroButtonWeight.Regular;
            this.btnPlugins.Location = new System.Drawing.Point(23, 220);
            this.btnPlugins.Name = "btnPlugins";
            this.btnPlugins.Size = new System.Drawing.Size(100, 23);
            this.btnPlugins.TabIndex = 32;
            this.btnPlugins.Text = "Open plugins";
            this.btnPlugins.UseSelectable = true;
            this.btnPlugins.Click += new System.EventHandler(this.btnPlugins_Click);
            // 
            // btnInstall
            // 
            this.btnInstall.FontSize = MetroFramework.MetroButtonSize.Tall;
            this.btnInstall.FontWeight = MetroFramework.MetroButtonWeight.Regular;
            this.btnInstall.Location = new System.Drawing.Point(341, 236);
            this.btnInstall.Name = "btnInstall";
            this.btnInstall.Size = new System.Drawing.Size(115, 36);
            this.btnInstall.TabIndex = 30;
            this.btnInstall.Text = "INSTALL";
            this.btnInstall.UseSelectable = true;
            this.btnInstall.Click += new System.EventHandler(this.btnInstall_Click);
            // 
            // linkRepo
            // 
            this.linkRepo.AutoSize = true;
            this.linkRepo.Font = new System.Drawing.Font("Segoe UI", 9F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
            this.linkRepo.LinkColor = System.Drawing.Color.FromArgb(((int)(((byte)(0)))), ((int)(((byte)(122)))), ((int)(((byte)(204)))));
            this.linkRepo.Location = new System.Drawing.Point(320, 40);
            this.linkRepo.Name = "linkRepo";
            this.linkRepo.Size = new System.Drawing.Size(136, 15);
            this.linkRepo.TabIndex = 35;
            this.linkRepo.TabStop = true;
            this.linkRepo.Text = "nomi-san/league-loader";
            this.linkRepo.LinkClicked += new System.Windows.Forms.LinkLabelLinkClickedEventHandler(this.linkRepo_LinkClicked);
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
            // metroPanel1
            // 
            this.metroPanel1.HorizontalScrollbarBarColor = true;
            this.metroPanel1.HorizontalScrollbarHighlightOnWheel = false;
            this.metroPanel1.HorizontalScrollbarSize = 10;
            this.metroPanel1.Location = new System.Drawing.Point(0, 63);
            this.metroPanel1.Name = "metroPanel1";
            this.metroPanel1.Size = new System.Drawing.Size(480, 230);
            this.metroPanel1.TabIndex = 39;
            this.metroPanel1.VerticalScrollbarBarColor = true;
            this.metroPanel1.VerticalScrollbarHighlightOnWheel = false;
            this.metroPanel1.VerticalScrollbarSize = 10;
            // 
            // GUI
            // 
            this.AutoScaleDimensions = new System.Drawing.SizeF(6F, 13F);
            this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
            this.ClientSize = new System.Drawing.Size(479, 295);
            this.Controls.Add(this.label1);
            this.Controls.Add(this.label2);
            this.Controls.Add(this.linkRepo);
            this.Controls.Add(this.linkRemote);
            this.Controls.Add(this.btnOpenDevTools);
            this.Controls.Add(this.btnPlugins);
            this.Controls.Add(this.btnInstall);
            this.Controls.Add(this.panelMain);
            this.Controls.Add(this.label3);
            this.Controls.Add(this.metroPanel1);
            this.Icon = ((System.Drawing.Icon)(resources.GetObject("$this.Icon")));
            this.MaximizeBox = false;
            this.Name = "GUI";
            this.Resizable = false;
            this.ShadowType = MetroFramework.Forms.MetroFormShadowType.AeroShadow;
            this.Text = "League Loader";
            this.Load += new System.EventHandler(this.GUI_Load);
            this.panelMain.ResumeLayout(false);
            this.panelMain.PerformLayout();
            this.ResumeLayout(false);
            this.PerformLayout();

        }

        #endregion

        private MetroFramework.Controls.MetroButton btnSelectPath;
        private MetroFramework.Controls.MetroTextBox textPath;
        private MetroFramework.Controls.MetroLabel metroLabel3;
        private MetroFramework.Controls.MetroLabel metroLabel2;
        private MetroFramework.Controls.MetroCheckBox checkDWS;
        private MetroFramework.Controls.MetroCheckBox checkICE;
        private MetroFramework.Controls.MetroTextBox textPort;
        private MetroFramework.Controls.MetroLabel metroLabel1;
        private MetroFramework.Controls.MetroRadioButton radioOff;
        private MetroFramework.Controls.MetroRadioButton radioPort;
        private MetroFramework.Controls.MetroPanel panelMain;
        private MetroFramework.Controls.MetroLink linkRemote;
        private MetroFramework.Controls.MetroButton btnOpenDevTools;
        private MetroFramework.Controls.MetroButton btnPlugins;
        private MetroFramework.Controls.MetroButton btnInstall;
        private System.Windows.Forms.LinkLabel linkRepo;
        private System.Windows.Forms.Label label3;
        private System.Windows.Forms.Label label1;
        private System.Windows.Forms.Label label2;
        private MetroFramework.Controls.MetroPanel metroPanel1;
    }
}

