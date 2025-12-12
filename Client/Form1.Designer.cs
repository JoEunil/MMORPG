namespace Client
{
    partial class Form1
    {
        /// <summary>
        ///  Required designer variable.
        /// </summary>
        private System.ComponentModel.IContainer components = null;

        /// <summary>
        ///  Clean up any resources being used.
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
        ///  Required method for Designer support - do not modify
        ///  the contents of this method with the code editor.
        /// </summary>
        private void InitializeComponent()
        {
            txtID = new TextBox();
            txtPW = new TextBox();
            txtChat = new TextBox();
            txtMsgInput = new TextBox();
            btnLogin = new Button();
            txtLog = new TextBox();
            ChannelList = new ListBox();
            label1 = new Label();
            label2 = new Label();
            label3 = new Label();
            btnEnter = new Button();
            SuspendLayout();
            // 
            // txtID
            // 
            txtID.Location = new Point(12, 43);
            txtID.Margin = new Padding(2);
            txtID.Name = "txtID";
            txtID.Size = new Size(104, 23);
            txtID.TabIndex = 0;
            // 
            // txtPW
            // 
            txtPW.Location = new Point(12, 68);
            txtPW.Margin = new Padding(2);
            txtPW.Name = "txtPW";
            txtPW.Size = new Size(104, 23);
            txtPW.TabIndex = 1;
            // 
            // txtChat
            // 
            txtChat.Location = new Point(12, 165);
            txtChat.Margin = new Padding(2);
            txtChat.Multiline = true;
            txtChat.Name = "txtChat";
            txtChat.Size = new Size(166, 124);
            txtChat.TabIndex = 2;
            // 
            // txtMsgInput
            // 
            txtMsgInput.Location = new Point(12, 290);
            txtMsgInput.Margin = new Padding(2);
            txtMsgInput.Name = "txtMsgInput";
            txtMsgInput.Size = new Size(166, 23);
            txtMsgInput.TabIndex = 3;
            // 
            // btnLogin
            // 
            btnLogin.Location = new Point(120, 45);
            btnLogin.Margin = new Padding(2);
            btnLogin.Name = "btnLogin";
            btnLogin.Size = new Size(58, 40);
            btnLogin.TabIndex = 4;
            btnLogin.Text = "button1";
            btnLogin.UseVisualStyleBackColor = true;
            // 
            // txtLog
            // 
            txtLog.BackColor = SystemColors.Window;
            txtLog.ForeColor = SystemColors.ControlText;
            txtLog.Location = new Point(203, 43);
            txtLog.Margin = new Padding(2);
            txtLog.Multiline = true;
            txtLog.Name = "txtLog";
            txtLog.Size = new Size(239, 270);
            txtLog.TabIndex = 5;
            // 
            // ChannelList
            // 
            ChannelList.FormattingEnabled = true;
            ChannelList.ItemHeight = 15;
            ChannelList.Location = new Point(12, 126);
            ChannelList.Name = "ChannelList";
            ChannelList.Size = new Size(104, 34);
            ChannelList.TabIndex = 7;
            // 
            // label1
            // 
            label1.Font = new Font("맑은 고딕", 12F, FontStyle.Bold, GraphicsUnit.Point, 129);
            label1.Location = new Point(12, 20);
            label1.Name = "label1";
            label1.Size = new Size(100, 23);
            label1.TabIndex = 0;
            label1.Text = "Login";
            // 
            // label2
            // 
            label2.Font = new Font("맑은 고딕", 12F, FontStyle.Bold, GraphicsUnit.Point, 129);
            label2.Location = new Point(12, 100);
            label2.Name = "label2";
            label2.Size = new Size(100, 23);
            label2.TabIndex = 8;
            label2.Text = "Chat";
            // 
            // label3
            // 
            label3.Font = new Font("맑은 고딕", 12F, FontStyle.Bold, GraphicsUnit.Point, 129);
            label3.Location = new Point(203, 20);
            label3.Name = "label3";
            label3.Size = new Size(100, 23);
            label3.TabIndex = 9;
            label3.Text = "Log";
            // 
            // btnEnter
            // 
            btnEnter.Location = new Point(120, 126);
            btnEnter.Name = "btnEnter";
            btnEnter.Size = new Size(58, 34);
            btnEnter.TabIndex = 10;
            btnEnter.Text = "button3";
            btnEnter.UseVisualStyleBackColor = true;
            // 
            // Form1
            // 
            AutoScaleDimensions = new SizeF(7F, 15F);
            AutoScaleMode = AutoScaleMode.Font;
            ClientSize = new Size(453, 618);
            Controls.Add(btnEnter);
            Controls.Add(label3);
            Controls.Add(label2);
            Controls.Add(label1);
            Controls.Add(ChannelList);
            Controls.Add(txtLog);
            Controls.Add(btnLogin);
            Controls.Add(txtMsgInput);
            Controls.Add(txtChat);
            Controls.Add(txtPW);
            Controls.Add(txtID);
            Margin = new Padding(2);
            Name = "Form1";
            Text = "Form1";
            Load += Form1_Load;
            ResumeLayout(false);
            PerformLayout();
        }

        #endregion

        private TextBox txtID;
        private TextBox txtPW;
        private TextBox txtChat;
        private TextBox txtMsgInput;
        private Button btnLogin;
        private TextBox txtLog;
        private ListBox ChannelList;
        private Label label1;
        private Label label2;
        private Label label3;
        private Button btnEnter;
    }
}
