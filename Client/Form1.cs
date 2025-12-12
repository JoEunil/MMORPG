using ClientCore;
using Microsoft.VisualBasic.ApplicationServices;
using System.Collections;
using System.ComponentModel;
using System.Diagnostics;
using System.Linq.Expressions;
using System.Net.Sockets;
using System.Runtime.InteropServices;
using System.Text;
using System.Text.Json;
using System.Xml.Serialization;
using static System.Net.Mime.MediaTypeNames;
using static System.Runtime.InteropServices.JavaScript.JSType;
using static System.Windows.Forms.VisualStyles.VisualStyleElement.Tab;

namespace Client
{
    public partial class Form1 : Form
    {

        private ViewModel _viewModel;
        const string IdPlaceholder = "아이디";
        const string PwPlaceholder = "비밀번호";
        public Form1()
        {
            InitializeComponent();

            _viewModel = new ViewModel(UILogger.Instance);

            TextBox[] txtList;
            txtList = new TextBox[] { txtID, txtPW };

            foreach (var txt in txtList)
            {
                //처음 공백 Placeholder 지정
                txt.ForeColor = Color.DarkGray;
                if (txt == txtID) txt.Text = IdPlaceholder;
                else if (txt == txtPW) txt.Text = PwPlaceholder;
                //텍스트박스 커서 Focus 여부에 따라 이벤트 지정
                txt.GotFocus += RemovePlaceholder;
                txt.LostFocus += SetPlaceholder;
            }

            btnLogin.Click += (s, e) => _viewModel.Login(txtID.Text, txtPW.Text);
            btnEnter.Click += (s, e) => _viewModel.Enter(ChannelList.SelectedIndex);
            txtMsgInput.KeyDown += (s, e) =>
            {
                if (e.KeyCode == Keys.Enter)
                { 
                    _viewModel.Chat(txtMsgInput.Text);
                    txtMsgInput.Clear();
                    e.Handled = true;
                }
            };

            _viewModel.OnConnectSuccess += updateChannelList;
            _viewModel.OnChatReceived += updateChat;
            _viewModel.OnChannelChanged += updateChannel;
            UILogger.Instance.LogToUI += OnLogReceived;
        }

        private void Form1_Load(object send, EventArgs e)
        {
            btnLogin.Text = "Login";
            btnEnter.Text = "Enter";
            btnEnter.Enabled = false;
            txtChat.ReadOnly = true;
            txtMsgInput.ReadOnly = true;
        }

        private void RemovePlaceholder(object sender, EventArgs e)
        {
            TextBox txt = (TextBox)sender;
            if (txt.Text == IdPlaceholder | txt.Text == PwPlaceholder)
            { //텍스트박스 내용이 사용자가 입력한 값이 아닌 Placeholder일 경우에만, 커서 포커스일때 빈칸으로 만들기
                txt.ForeColor = Color.Black; //사용자 입력 진한 글씨
                txt.Text = string.Empty;
                if (txt == txtPW) txtPW.PasswordChar = '●';
            }
        }

        private void SetPlaceholder(object sender, EventArgs e)
        {
            TextBox txt = (TextBox)sender;
            if (string.IsNullOrWhiteSpace(txt.Text))
            {
                //사용자 입력값이 하나도 없는 경우에 포커스 잃으면 Placeholder 적용해주기
                txt.ForeColor = Color.DarkGray; //Placeholder 흐린 글씨
                if (txt == txtID) txt.Text = IdPlaceholder;
                else if (txt == txtPW) { txt.Text = PwPlaceholder; txtPW.PasswordChar = default; }
            }
        }
        private void updateChannelList(ChannelList list)
        {
            btnEnter.Enabled = true;
            ChannelList.Items.Clear();
            foreach (var channel in list.channels)
            {
                ChannelList.Items.Add(channel.name + ": " + channel.userCount);
            }
            btnEnter.Enabled = true;
        }
        private void updateChat(string message)
        {
            txtChat.AppendText(message + Environment.NewLine);
            txtChat.ScrollToCaret(); //스크롤을 맨 아래로 이동
        }

        private void updateChannel(int channel)
        {
            txtChat.Clear();

            if (channel == 0)
            {
                txtChat.Text = $"로비 채널에 입장했습니다." + Environment.NewLine;
                txtMsgInput.ReadOnly = true;
            }
            else
            {
                txtChat.Text = $"채널 {channel}에 입장했습니다." + Environment.NewLine;
                txtMsgInput.ReadOnly = false;
            }
        }
        private void OnLogReceived(string logMessage)
        {
            txtLog.AppendText(logMessage);
        }
    }
}
