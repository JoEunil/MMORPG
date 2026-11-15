namespace ServerDependencyAnalyzer;

internal sealed class AnalyzerForm : Form
{
    private readonly TextBox folder = new() { ReadOnly = true, Dock = DockStyle.Fill, AccessibleName = "분석 폴더" };
    private readonly ComboBox project = MakeCombo("진입 프로젝트");
    private readonly ComboBox configuration = MakeCombo("구성");
    private readonly ComboBox platform = MakeCombo("플랫폼");
    private readonly Button browse = new() { Text = "폴더 선택…", AutoSize = true };
    private readonly Button browseDatabase = new() { Text = "빌드 DB 선택…", AutoSize = true, Enabled = false };
    private readonly CheckBox tests = new() { Text = "테스트 포함", AutoSize = true };
    private readonly CheckBox vendor = new() { Text = "외부 라이브러리 표시", AutoSize = true, Checked = true };
    private readonly Button analyze = new() { Text = "분석", AutoSize = true, Enabled = false };
    private readonly Button cancel = new() { Text = "취소", AutoSize = true, Enabled = false };
    private readonly Button open = new() { Text = "결과 열기", AutoSize = true, Enabled = false };
    private readonly Button openReports = new() { Text = "결과 폴더", AutoSize = true };
    private readonly Label status = new() { AutoSize = true, Text = "분석할 C++ 프로젝트 폴더를 선택하세요." };
    private readonly ProgressBar progressBar = new() { Dock = DockStyle.Fill, Height = 7, Visible = false };
    private CancellationTokenSource? activeRun;
    private string? lastReport;

    public AnalyzerForm()
    {
        Text = "C/C++ 의존성 분석기";
        Font = new Font("Malgun Gothic", 10F);
        ClientSize = new Size(760, 370);
        MinimumSize = new Size(650, 395);
        StartPosition = FormStartPosition.CenterScreen;
        AutoScaleMode = AutoScaleMode.Dpi;

        configuration.Items.AddRange(["Debug", "Release"]);
        configuration.SelectedIndex = 0;
        configuration.DropDownStyle = ComboBoxStyle.DropDown;
        platform.Items.AddRange(["x64", "Win32", "ARM64"]);
        platform.SelectedIndex = 0;
        platform.DropDownStyle = ComboBoxStyle.DropDown;
        configuration.Width = platform.Width = 105;

        var layout = new TableLayoutPanel
        {
            Dock = DockStyle.Fill, ColumnCount = 3, RowCount = 8, Padding = new Padding(20)
        };
        layout.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute, 128));
        layout.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100));
        layout.ColumnStyles.Add(new ColumnStyle(SizeType.AutoSize));
        for (var row = 0; row < 7; row++)
            layout.RowStyles.Add(new RowStyle(SizeType.AutoSize));
        layout.RowStyles.Add(new RowStyle(SizeType.Percent, 100));
        Controls.Add(layout);

        var description = MakeLabel("C/C++ 소스를 Clang으로 읽어 클래스와 프로젝트의 의존성을 브라우저 지도에 표시합니다.");
        description.Margin = new Padding(3, 2, 3, 12);
        layout.Controls.Add(description, 0, 0);
        layout.SetColumnSpan(description, 3);
        layout.Controls.Add(MakeLabel("분석 폴더"), 0, 1);
        layout.Controls.Add(folder, 1, 1);
        layout.Controls.Add(browse, 2, 1);
        layout.Controls.Add(MakeLabel("분석 설정"), 0, 2);
        layout.Controls.Add(project, 1, 2);
        layout.Controls.Add(browseDatabase, 2, 2);
        project.Dock = DockStyle.Fill;
        layout.Controls.Add(MakeLabel("빌드 설정"), 0, 3);
        var buildOptions = MakeRow(configuration, platform);
        layout.Controls.Add(buildOptions, 1, 3);
        layout.SetColumnSpan(buildOptions, 2);
        var scopeOptions = MakeRow(tests, vendor);
        layout.Controls.Add(MakeLabel("포함 범위"), 0, 4);
        layout.Controls.Add(scopeOptions, 1, 4);
        layout.SetColumnSpan(scopeOptions, 2);
        var actions = MakeRow(analyze, cancel, open, openReports);
        layout.Controls.Add(actions, 1, 5);
        layout.SetColumnSpan(actions, 2);
        progressBar.Margin = new Padding(3, 10, 3, 4);
        layout.Controls.Add(progressBar, 0, 6);
        layout.SetColumnSpan(progressBar, 3);
        layout.Controls.Add(status, 0, 7);
        layout.SetColumnSpan(status, 3);
        status.Margin = new Padding(3, 10, 3, 3);
        layout.SizeChanged += (_, _) => description.MaximumSize = status.MaximumSize =
            new Size(Math.Max(300, layout.ClientSize.Width - layout.Padding.Horizontal - 6), 0);

        browse.Click += (_, _) => SelectFolder();
        browseDatabase.Click += (_, _) => SelectDatabase();
        project.SelectedIndexChanged += (_, _) => UpdateBuildInputs();
        tests.CheckedChanged += (_, _) =>
        {
            if (folder.Text.Length > 0)
                LoadInputs(folder.Text, project.SelectedItem as ProjectChoice);
        };
        analyze.Click += async (_, _) => await Analyze();
        cancel.Click += (_, _) => CancelAnalysis();
        open.Click += (_, _) => OpenLastReport();
        openReports.Click += (_, _) => OpenReportsFolder();
        FormClosing += (_, args) =>
        {
            if (activeRun is null)
                return;
            args.Cancel = true;
            CancelAnalysis();
        };
    }

    private void SelectFolder()
    {
        using var dialog = new FolderBrowserDialog
        {
            Description = "분석할 C++ 프로젝트 폴더를 선택하세요.",
            UseDescriptionForTitle = true,
            SelectedPath = folder.Text
        };
        if (dialog.ShowDialog(this) != DialogResult.OK)
            return;

        LoadInputs(dialog.SelectedPath, null);
    }

    private void LoadInputs(string directory, ProjectChoice? selection)
    {
        try
        {
            var databases = CompilationDatabase.Discover(directory);
            var projects = ProjectFiles.Discover(directory, tests.Checked);
            folder.Text = directory;
            project.Items.Clear();
            foreach (var path in databases)
                project.Items.Add(new ProjectChoice("컴파일 DB · " + Path.GetRelativePath(directory, path), path));
            project.Items.Add(new ProjectChoice(projects.Length == 0 ? "폴더의 C/C++ 파일" : "찾은 Visual Studio C++ 프로젝트", null));
            foreach (var path in projects)
                project.Items.Add(new ProjectChoice(Path.GetRelativePath(directory, path), path));
            var selectedPath = selection?.Path;
            if (CompilationDatabase.IsDatabase(selectedPath) && !databases.Contains(selectedPath!, StringComparer.OrdinalIgnoreCase))
                project.Items.Add(new ProjectChoice("컴파일 DB · " + selectedPath, selectedPath));
            var index = selection is null ? 0 : project.Items.Cast<ProjectChoice>().ToList()
                .FindIndex(choice => string.Equals(choice.Path, selectedPath, StringComparison.OrdinalIgnoreCase));
            project.SelectedIndex = Math.Max(0, index);
            SetBusy(false);
            status.Text = databases.Length == 0 && projects.Length == 0
                ? "빌드 설정 파일을 선택하거나 폴더의 C/C++ 파일을 분석하세요."
                : $"컴파일 DB {databases.Length}개 · Visual Studio 프로젝트 {projects.Length}개";
        }
        catch (Exception exception) when (exception is IOException or UnauthorizedAccessException)
        {
            status.Text = exception.Message;
        }
    }

    private void SelectDatabase()
    {
        using var dialog = new OpenFileDialog
        {
            Title = "컴파일 데이터베이스 선택",
            Filter = "컴파일 데이터베이스 (*.json)|*.json",
            FileName = "compile_commands.json",
            InitialDirectory = folder.Text
        };
        if (dialog.ShowDialog(this) == DialogResult.OK)
            LoadInputs(folder.Text, new ProjectChoice("컴파일 DB", dialog.FileName));
    }

    private async Task Analyze()
    {
        using var run = new CancellationTokenSource();
        activeRun = run;
        lastReport = null;
        var options = new AnalysisOptions(folder.Text, (project.SelectedItem as ProjectChoice)?.Path,
            configuration.Text, platform.Text, IncludeTests: tests.Checked, ShowExternalLibraries: vendor.Checked,
            DiscoverDatabase: false);
        SetBusy(true);
        var progress = new Progress<string>(message => { if (activeRun == run && !run.IsCancellationRequested) status.Text = message; });
        try
        {
            var (result, report) = await Task.Run(() =>
            {
                var result = Analyzer.Run(options, progress, run.Token);
                run.Token.ThrowIfCancellationRequested();
                return (result, HtmlReport.Write(result, null));
            }, run.Token);
            lastReport = report;
            status.Text = $"오류 없이 분석한 파일 {result.Parsed}/{result.Sources}개 · 진단 {result.Failures.Count}건. 결과를 브라우저에서 엽니다.";
            OpenLastReport();
        }
        catch (OperationCanceledException) { status.Text = "분석을 취소했습니다."; }
        catch (Exception exception) { status.Text = $"분석 결과를 만들지 못했습니다. {exception.Message}"; }
        finally
        {
            activeRun = null;
            SetBusy(false);
        }
    }

    private void OpenLastReport()
    {
        if (lastReport is null)
            return;
        try { Program.OpenPath(lastReport); }
        catch (Exception exception) { status.Text = $"브라우저를 열지 못했습니다. {lastReport}\n{exception.Message}"; }
    }

    private void OpenReportsFolder()
    {
        try { Program.OpenPath(Directory.CreateDirectory(HtmlReport.OutputDirectory).FullName); }
        catch (Exception exception) { status.Text = $"결과 폴더를 열지 못했습니다. {exception.Message}"; }
    }

    private void CancelAnalysis()
    {
        activeRun?.Cancel();
        cancel.Enabled = false;
        status.Text = "현재 파일 분석을 마치는 대로 취소합니다…";
    }

    private void SetBusy(bool busy)
    {
        browse.Enabled = tests.Enabled = vendor.Enabled = !busy;
        browseDatabase.Enabled = project.Enabled = analyze.Enabled = !busy && folder.Text.Length > 0;
        UpdateBuildInputs();
        cancel.Enabled = busy;
        open.Enabled = !busy && lastReport is not null;
        progressBar.Visible = busy;
        progressBar.Style = busy ? ProgressBarStyle.Marquee : ProgressBarStyle.Blocks;
    }

    private void UpdateBuildInputs() => configuration.Enabled = platform.Enabled = activeRun is null
        && !CompilationDatabase.IsDatabase((project.SelectedItem as ProjectChoice)?.Path);

    private static ComboBox MakeCombo(string name) => new() { DropDownStyle = ComboBoxStyle.DropDownList, AccessibleName = name };
    private static Label MakeLabel(string text) => new() { Text = text, Anchor = AnchorStyles.Left, AutoSize = true };
    private static FlowLayoutPanel MakeRow(params Control[] controls)
    {
        var row = new FlowLayoutPanel { AutoSize = true, WrapContents = false, Margin = Padding.Empty };
        row.Controls.AddRange(controls);
        return row;
    }

    private sealed record ProjectChoice(string Label, string? Path)
    {
        public override string ToString() => Label;
    }
}
