using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Drawing;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Windows.Forms;

namespace EnhancedSetup {
    public static class WindowsArguments {
        // Microsoft C-runtime quoting: preserve quotes and trailing backslashes.
        public static string Quote(string value) {
            if(value==null)throw new ArgumentNullException("value");
            var result=new StringBuilder("\"");int slashes=0;
            foreach(char c in value) {
                if(c=='\\'){slashes++;continue;}
                result.Append('\\',slashes*(c=='"'?2:1));slashes=0;
                if(c=='"')result.Append('\\');result.Append(c);
            }
            result.Append('\\',slashes*2);return result.Append('"').ToString();
        }
    }
    internal sealed class InstallResult {
        public bool Success;
        public string Message, Log, Launcher, Python;
    }
    internal sealed class SetupWindow : Form {
        readonly Color ink=Color.FromArgb(18,21,23), panel=Color.FromArgb(29,33,36);
        readonly Color text=Color.FromArgb(235,230,219), gold=Color.FromArgb(218,174,101);
        readonly string distribution=Path.GetFullPath(AppDomain.CurrentDomain.BaseDirectory);
        readonly TextBox game=new TextBox(), python=new TextBox(), output=new TextBox();
        readonly Label status=new Label(), requirements=new Label();
        readonly Button install=new Button(), close=new Button(), browseGame=new Button(), browsePython=new Button(), openFolder=new Button(), manual=new Button();
        readonly ProgressBar progress=new ProgressBar();
        bool busy, finished, releaseReady;
        string logPath, installedGame;

        public SetupWindow() {
            Text="Alone in the Dark - Enhanced Setup";
            BackColor=ink;ForeColor=text;Font=new Font("Segoe UI",10F);
            AutoScaleDimensions=new SizeF(96F,96F);AutoScaleMode=AutoScaleMode.Dpi;
            ClientSize=new Size(900,740);MinimumSize=new Size(740,650);StartPosition=FormStartPosition.CenterScreen;
            try { Icon=Icon.ExtractAssociatedIcon(Application.ExecutablePath); } catch(ArgumentException) { }
            var root=new TableLayoutPanel { Dock=DockStyle.Fill,AutoScroll=true,Padding=new Padding(28),ColumnCount=1,RowCount=6,BackColor=ink };
            root.RowStyles.Add(new RowStyle(SizeType.AutoSize));
            root.RowStyles.Add(new RowStyle(SizeType.AutoSize));
            root.RowStyles.Add(new RowStyle(SizeType.AutoSize));
            root.RowStyles.Add(new RowStyle(SizeType.Percent,100));
            root.RowStyles.Add(new RowStyle(SizeType.AutoSize));
            root.RowStyles.Add(new RowStyle(SizeType.AutoSize));
            Controls.Add(root);
            var header=new FlowLayoutPanel { Dock=DockStyle.Fill,BackColor=ink,AutoSize=true,FlowDirection=FlowDirection.TopDown,WrapContents=false,Margin=new Padding(0,0,0,18) };
            var title=new Label { Text="ALONE IN THE DARK",AutoSize=true,Margin=new Padding(0,0,0,6),Font=new Font("Georgia",24F,FontStyle.Bold),ForeColor=gold };
            var subtitle=new Label { Text="ENHANCED PC EDITION  /  SETUP",AutoSize=true,Font=new Font("Segoe UI",11F,FontStyle.Bold),Margin=new Padding(0,0,0,5) };
            var detail=new Label { Text="An unofficial mod for your original Steam installation.",AutoSize=true,ForeColor=Color.FromArgb(174,179,179) };
            header.Controls.AddRange(new Control[]{title,subtitle,detail});root.Controls.Add(header,0,0);

            var choices=new TableLayoutPanel { Dock=DockStyle.Fill,AutoSize=true,ColumnCount=2,RowCount=6,BackColor=ink,Margin=new Padding(0) };
            choices.ColumnStyles.Add(new ColumnStyle(SizeType.Percent,100));choices.ColumnStyles.Add(new ColumnStyle(SizeType.AutoSize));
            for(int row=0;row<6;row++)choices.RowStyles.Add(new RowStyle(SizeType.AutoSize));
            ConfigureField(game);ConfigureField(python);ConfigureButton(browseGame,"Browse...");ConfigureButton(browsePython,"Browse...");
            var gameLabel=FieldLabel("Game folder");choices.Controls.Add(gameLabel,0,0);choices.SetColumnSpan(gameLabel,2);
            choices.Controls.Add(game,0,1);choices.Controls.Add(browseGame,1,1);
            var gameHint=FieldLabel("The folder containing the original Alone.exe. Close the game first.");gameHint.Font=new Font("Segoe UI",8.5F);gameHint.ForeColor=Color.Silver;choices.Controls.Add(gameHint,0,2);choices.SetColumnSpan(gameHint,2);
            var pythonLabel=FieldLabel("Python (leave empty to set it up automatically)");choices.Controls.Add(pythonLabel,0,3);choices.SetColumnSpan(pythonLabel,2);
            choices.Controls.Add(python,0,4);choices.Controls.Add(browsePython,1,4);
            var pythonHint=FieldLabel("Missing requirements? Setup will ask before downloading anything.");pythonHint.Font=new Font("Segoe UI",8.5F);pythonHint.ForeColor=Color.Silver;choices.Controls.Add(pythonHint,0,5);choices.SetColumnSpan(pythonHint,2);
            root.Controls.Add(choices,0,1);
            requirements.Dock=DockStyle.Fill;requirements.AutoSize=true;requirements.Text="Required: 64-bit Python 3.12+, Pillow and NumPy.\r\nDownloads are optional. Original game files and activation are preserved.";requirements.ForeColor=Color.FromArgb(183,186,181);requirements.Padding=new Padding(0,12,0,12);root.Controls.Add(requirements,0,2);

            output.Dock=DockStyle.Fill;output.MinimumSize=new Size(0,100);output.Multiline=true;output.ReadOnly=true;output.ScrollBars=ScrollBars.Vertical;
            output.BackColor=panel;output.ForeColor=text;output.BorderStyle=BorderStyle.FixedSingle;output.Font=new Font("Consolas",9F);output.WordWrap=true;root.Controls.Add(output,0,3);
            var statusArea=new TableLayoutPanel { Dock=DockStyle.Fill,ColumnCount=1,RowCount=2,Padding=new Padding(0,8,0,0) };
            statusArea.AutoSize=true;statusArea.RowStyles.Add(new RowStyle(SizeType.AutoSize));statusArea.RowStyles.Add(new RowStyle(SizeType.Absolute,8));status.Dock=DockStyle.Fill;status.AutoSize=true;status.Padding=new Padding(0,0,0,8);status.Text="Ready to check your selected folders.";progress.Dock=DockStyle.Fill;progress.Style=ProgressBarStyle.Blocks;
            statusArea.Controls.Add(status,0,0);statusArea.Controls.Add(progress,0,1);root.Controls.Add(statusArea,0,4);
            var buttons=new FlowLayoutPanel { Dock=DockStyle.Fill,AutoSize=true,FlowDirection=FlowDirection.RightToLeft,WrapContents=false,Padding=new Padding(0,10,0,0) };
            ConfigureButton(close,"Close");ConfigureButton(install,"Install mod");ConfigureButton(openFolder,"Open game folder");
            install.BackColor=gold;install.ForeColor=Color.FromArgb(27,25,21);install.Font=new Font(Font,FontStyle.Bold);install.Width=142;openFolder.Width=160;openFolder.Visible=false;
            ConfigureButton(manual,"Manual downloads");manual.Click+=delegate { Process.Start(new ProcessStartInfo("https://github.com/evilive04/aitd2008-enhanced-pc/blob/main/README-INSTALL.md#manual-requirements") { UseShellExecute=true }); };
            buttons.Controls.Add(close);buttons.Controls.Add(install);buttons.Controls.Add(openFolder);buttons.Controls.Add(manual);root.Controls.Add(buttons,0,5);
            browseGame.Click+=BrowseGame;browsePython.Click+=BrowsePython;install.Click+=InstallClicked;close.Click+=delegate { Close(); };
            openFolder.Click+=delegate { if(!String.IsNullOrEmpty(installedGame))Process.Start(new ProcessStartInfo(Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.Windows),"explorer.exe"),WindowsArguments.Quote(installedGame)) { UseShellExecute=false }); };
            FormClosing+=ClosingWhileBusy;
            Shown+=delegate {
                Rectangle area=Screen.FromControl(this).WorkingArea;
                MinimumSize=new Size(Math.Min(MinimumSize.Width,area.Width),Math.Min(MinimumSize.Height,area.Height));
                Size=new Size(Math.Min(Width,area.Width),Math.Min(Height,area.Height));
                Location=new Point(Math.Max(area.Left,Math.Min(Left,area.Right-Width)),Math.Max(area.Top,Math.Min(Top,area.Bottom-Height)));
            };
            game.Text=DetectGame();python.Text=DetectPython();
            releaseReady=File.Exists(Path.Combine(distribution,"tools","install_mod.py"))&&File.Exists(Path.Combine(distribution,"PAYLOAD_MANIFEST.json"))&&Directory.Exists(Path.Combine(distribution,"payload"));
            if(!releaseReady){
                install.Enabled=false;status.Text="A packaged Release is required.";status.ForeColor=gold;
                output.Text="This folder does not contain an installable Release payload.\r\n\r\nDownload a packaged Release, or build the Release payload from the source first. Keep this setup executable beside PAYLOAD_MANIFEST.json, the payload folder and the tools folder.\r\n\r\nNo game files have been changed.";
            }else output.Text="Start and activate your original game normally before installing.\r\nChoose a clean game folder, then select Install mod.\r\n\r\nIf Python or its required packages are missing, you can approve automatic setup. No downloads take place without your consent.\r\nExisting mod files are never overwritten.\r\n";
        }
        Label FieldLabel(string value) { return new Label { Text=value,AutoSize=true,Dock=DockStyle.Fill,TextAlign=ContentAlignment.MiddleLeft,Margin=new Padding(0,5,0,4) }; }
        void ConfigureField(TextBox box) { box.Dock=DockStyle.Fill;box.Margin=new Padding(0,5,10,5);box.BackColor=panel;box.ForeColor=text;box.BorderStyle=BorderStyle.FixedSingle; }
        void ConfigureButton(Button button,string value) { button.Text=value;button.AutoSize=true;button.AutoSizeMode=AutoSizeMode.GrowAndShrink;button.MinimumSize=new Size(104,36);button.Padding=new Padding(12,6,12,6);button.Margin=new Padding(6,2,0,2);button.BackColor=panel;button.ForeColor=text;button.FlatStyle=FlatStyle.Flat;button.FlatAppearance.BorderColor=Color.FromArgb(88,83,70);button.UseVisualStyleBackColor=false; }
        void BrowseGame(object sender,EventArgs e) {
            using(var dialog=new FolderBrowserDialog { Description="Choose the original game folder containing Alone.exe",ShowNewFolderButton=false,SelectedPath=Directory.Exists(game.Text)?game.Text:"" })if(dialog.ShowDialog(this)==DialogResult.OK)game.Text=dialog.SelectedPath;
        }
        void BrowsePython(object sender,EventArgs e) {
            using(var dialog=new OpenFileDialog { Title="Choose your 64-bit Python interpreter",Filter="Python interpreter (python.exe)|python.exe|Executable files (*.exe)|*.exe",CheckFileExists=true,FileName="python.exe" })if(dialog.ShowDialog(this)==DialogResult.OK)python.Text=dialog.FileName;
        }
        static string DetectGame() {
            var candidate=Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.ProgramFilesX86),"Steam","steamapps","common","Alone in the Dark");
            return File.Exists(Path.Combine(candidate,"Alone.exe"))?candidate:"";
        }
        static string DetectPython() {
            var candidates=new List<string>();
            string environments=Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),"AITD2008Enhanced","setup","environments");
            try { if(Directory.Exists(environments))foreach(string folder in Directory.GetDirectories(environments,"env-*").OrderByDescending(x=>Directory.GetLastWriteTimeUtc(x)))candidates.Add(Path.Combine(folder,"Scripts","python.exe")); }
            catch(IOException) { } catch(UnauthorizedAccessException) { }
            foreach(var root in new[]{Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),"Programs","Python"),Environment.GetFolderPath(Environment.SpecialFolder.ProgramFiles)}) {
                try { if(Directory.Exists(root))foreach(var folder in Directory.GetDirectories(root,"Python*").OrderByDescending(x=>x,StringComparer.OrdinalIgnoreCase))candidates.Add(Path.Combine(folder,"python.exe")); }
                catch(IOException) { } catch(UnauthorizedAccessException) { }
            }
            foreach(string folder in (Environment.GetEnvironmentVariable("PATH")??"").Split(Path.PathSeparator)) {
                if(String.IsNullOrWhiteSpace(folder)||folder.IndexOf("WindowsApps",StringComparison.OrdinalIgnoreCase)>=0)continue;
                try { candidates.Add(Path.Combine(folder.Trim().Trim('"'),"python.exe")); }catch(ArgumentException) { }
            }
            return candidates.FirstOrDefault(File.Exists)??"";
        }
        void AppendLog(string value) {
            if(IsDisposed||Disposing)return;
            if(InvokeRequired){try { BeginInvoke(new Action<string>(AppendLog),value); }catch(InvalidOperationException) { }return;}
            if(output.TextLength>262144)output.Text=output.Text.Substring(output.TextLength-131072);
            output.AppendText(value+Environment.NewLine);
        }
        void SetBusy(bool value) {
            busy=value;game.Enabled=!value&&!finished;python.Enabled=!value&&!finished;browseGame.Enabled=!value&&!finished;browsePython.Enabled=!value&&!finished;
            install.Enabled=!value&&!finished&&releaseReady;close.Enabled=!value;openFolder.Enabled=!value;manual.Enabled=!value;
            progress.Style=value?ProgressBarStyle.Marquee:ProgressBarStyle.Blocks;progress.MarqueeAnimationSpeed=value?28:0;
            if(!value)progress.Value=finished?100:0;
        }
        void ClosingWhileBusy(object sender,FormClosingEventArgs e) {
            if(!busy)return;e.Cancel=true;
            MessageBox.Show(this,"Installation is still running. Please wait for it to finish so any cleanup can complete safely. Setup has not stopped the installation.","Installation in progress",MessageBoxButtons.OK,MessageBoxIcon.Information);
        }
        async void InstallClicked(object sender,EventArgs e) {
            if(busy||finished||!releaseReady)return;
            string gameFolder,pythonFile;
            try {
                gameFolder=Path.GetFullPath(game.Text.Trim());pythonFile=String.IsNullOrWhiteSpace(python.Text)?"":Path.GetFullPath(python.Text.Trim());
                if(!File.Exists(Path.Combine(gameFolder,"Alone.exe")))throw new IOException("Choose the game folder containing the original Alone.exe.");
                if(pythonFile.Length!=0&&(!File.Exists(pythonFile)||!String.Equals(Path.GetExtension(pythonFile),".exe",StringComparison.OrdinalIgnoreCase)))throw new IOException("Choose an installed python.exe interpreter, or leave Python empty for automatic setup.");
            }catch(Exception error) {
                MessageBox.Show(this,error.Message,"Check the selected paths",MessageBoxButtons.OK,MessageBoxIcon.Information);return;
            }
            SetBusy(true);status.ForeColor=text;status.Text="Checking Python and preparing local assets...";output.Clear();
            InstallResult result;
            try { result=await Task.Run(()=>RunInstallation(gameFolder,pythonFile)); }
            catch(Exception error) { result=new InstallResult { Success=false,Message=error.Message,Log=logPath }; }
            if(!String.IsNullOrEmpty(result.Python))python.Text=result.Python;
            finished=result.Success;SetBusy(false);
            if(result.Success){
                installedGame=gameFolder;openFolder.Visible=true;status.Text="Installation complete. Your Enhanced launcher is ready.";status.ForeColor=Color.FromArgb(159,202,161);
                AppendLog("");AppendLog("READY TO PLAY");AppendLog(result.Launcher);AppendLog("Original game files, activation and saved games were preserved.");
                MessageBox.Show(this,"Installation complete.\r\n\r\nStart the mod with:\r\n"+result.Launcher,"Enhanced PC Edition",MessageBoxButtons.OK,MessageBoxIcon.Information);
            }else{
                status.Text="Installation did not complete. See the log above.";status.ForeColor=gold;
                AppendLog("");AppendLog(result.Message);if(!String.IsNullOrEmpty(result.Log))AppendLog("Full log: "+result.Log);
                MessageBox.Show(this,result.Message+(String.IsNullOrEmpty(result.Log)?"":"\r\n\r\nFull log:\r\n"+result.Log),"Setup could not complete",MessageBoxButtons.OK,MessageBoxIcon.Warning);
            }
        }
        PythonEnvironment ProbePython(string interpreter,Action<string> log) {
            if(String.IsNullOrEmpty(interpreter)||!File.Exists(interpreter))return new PythonEnvironment();
            string transcript;
            int exit=RunProcess(interpreter,new[]{"-I","-u","-X","utf8","-c",PythonEnvironment.Probe},log,out transcript);
            return PythonEnvironment.Parse(exit,transcript);
        }
        string EnsureRequirements(string interpreter,Action<string> log) {
            var environment=ProbePython(interpreter,log);
            if(environment.Packages)return interpreter;
            bool approve=(bool)Invoke(new Func<bool>(()=>MessageBox.Show(this,
                "Setup needs "+(environment.Interpreter?"Pillow 13.0.0 and NumPy 2.5.4.":"64-bit Python, Pillow 13.0.0 and NumPy 2.5.4.")+"\r\n\r\n"+
                "Download and install the missing requirements now?\r\n\r\n"+
                "Python is downloaded from python.org and its publisher signature is verified. Packages come from the official Python Package Index (PyPI) and are installed in a separate environment for this mod. Existing Python packages are not changed.\r\n\r\n"+
                "An Internet connection and disk space are required. No game files will be changed during this step. Select No to use Manual downloads instead.",
                "Set up requirements?",MessageBoxButtons.YesNo,MessageBoxIcon.Question,MessageBoxDefaultButton.Button2)==DialogResult.Yes));
            if(!approve)throw new IOException("Requirements setup was declined. Nothing was downloaded. Use Manual downloads, or select Python with the required packages and try again.");
            string setupRoot=Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),"AITD2008Enhanced","setup");
            if(!environment.Interpreter) {
                string detected=DetectPython();
                if(ProbePython(detected,log).Interpreter)interpreter=detected;
                else {
                    string target=Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),"Programs","Python","Python314");
                    if(Directory.Exists(target))throw new IOException("An existing Python folder needs attention. Use Manual downloads to repair Python, then select its python.exe. Setup will not overwrite that folder.");
                    string downloadFolder=Path.Combine(setupRoot,"downloads",Guid.NewGuid().ToString("N"));Directory.CreateDirectory(downloadFolder);
                    string installer=PythonDownload.Download(downloadFolder,log),ignored;
                    log("Installing Python for the current Windows user (without changing PATH)...");
                    int exit=RunProcess(installer,new[]{"/quiet","InstallAllUsers=0","PrependPath=0","Include_launcher=0","Include_test=0","Include_doc=0","Shortcuts=0","AssociateFiles=0","TargetDir="+target},log,out ignored);
                    if(exit!=0&&exit!=3010)throw new IOException("Python setup did not complete (exit "+exit+"). Use Manual downloads if necessary.");
                    interpreter=Path.Combine(target,"python.exe");
                    if(!ProbePython(interpreter,log).Interpreter)throw new IOException("Python setup completed but a supported interpreter was not found. Use Manual downloads.");
                }
            }
            string environmentPath=Path.Combine(setupRoot,"environments","env-"+Guid.NewGuid().ToString("N")),transcript;
            log("Creating an isolated mod environment...");
            if(RunProcess(interpreter,new[]{"-I","-m","venv",environmentPath},log,out transcript)!=0)throw new IOException("Could not create the mod's Python environment. Check disk space and use Manual downloads if needed.");
            string managed=Path.Combine(environmentPath,"Scripts","python.exe");
            log("Installing Pillow and NumPy from PyPI (prebuilt wheels only)...");
            if(RunProcess(managed,new[]{"-I","-m","pip","--isolated","install","--disable-pip-version-check","--no-input","--index-url","https://pypi.org/simple","--only-binary=:all:","--no-deps","Pillow==13.0.0","numpy==2.5.4"},log,out transcript)!=0)
                throw new IOException("The required packages could not be installed. Check the Internet connection, use Manual downloads, or try again. No game files were changed.");
            if(!ProbePython(managed,log).Packages)throw new IOException("Downloaded requirements did not pass the final version check. Installation stopped before changing game files.");
            log("Isolated requirements are ready.");return managed;
        }
        InstallResult RunInstallation(string gameFolder,string pythonFile) {
            string logs=Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),"AITD2008Enhanced","setup");Directory.CreateDirectory(logs);
            logPath=Path.Combine(logs,"setup-"+DateTime.UtcNow.ToString("yyyyMMdd-HHmmss")+"-"+Guid.NewGuid().ToString("N").Substring(0,8)+".log");
            using(var writer=new StreamWriter(new FileStream(logPath,FileMode.CreateNew,FileAccess.Write,FileShare.Read),new UTF8Encoding(false))) {
                writer.AutoFlush=true;object gate=new object();bool logFailure=false;
                Action<string> log=delegate(string line) {
                    lock(gate) {
                        if(!logFailure)try { writer.WriteLine(line); }
                        catch(IOException) { logFailure=true;AppendLog("The full log could not be updated. Check available disk space; the installation process has not been stopped."); }
                        catch(UnauthorizedAccessException) { logFailure=true;AppendLog("The full log could not be updated. The installation process has not been stopped."); }
                    }
                    AppendLog(line);
                };
                log("Enhanced PC Edition setup");log("Game folder: "+gameFolder);log("Checking selected Python and required packages...");
                pythonFile=EnsureRequirements(pythonFile,log);
                log("Python requirements confirmed. Preparing assets from your own game; this may take a few minutes.");
                string script=Path.Combine(distribution,"tools","install_mod.py"),ignored;
                int exit=RunProcess(pythonFile,new[]{"-E","-u","-X","utf8",script,gameFolder,"--distribution",distribution},log,out ignored);
                if(exit!=0)return new InstallResult { Message="The installer reported an error (exit code "+exit+"). Review the log for the exact cause. Use a clean supported game folder and check requirements-install.txt. The backend preserves pre-existing files and rolls back its own incomplete file copies.",Log=logPath };
                string launcher=Path.Combine(gameFolder,"AITD Enhanced Launcher.exe");
                if(!File.Exists(launcher))return new InstallResult { Message="The installer exited without the expected launcher. Keep the log and inspect the game folder before trying again.",Log=logPath };
                log("Installation complete: "+launcher);return new InstallResult { Success=true,Launcher=launcher,Python=pythonFile,Log=logPath,Message="Installation complete." };
            }
        }
        int RunProcess(string executable,IEnumerable<string> arguments,Action<string> log,out string transcript) {
            var captured=new StringBuilder();object captureGate=new object();
            var info=new ProcessStartInfo { FileName=executable,Arguments=String.Join(" ",arguments.Select(WindowsArguments.Quote)),WorkingDirectory=distribution,
                UseShellExecute=false,CreateNoWindow=true,RedirectStandardOutput=true,RedirectStandardError=true,StandardOutputEncoding=Encoding.UTF8,StandardErrorEncoding=Encoding.UTF8 };
            info.EnvironmentVariables["PYTHONUTF8"]="1";
            using(var child=new Process { StartInfo=info }) {
                DataReceivedEventHandler receive=delegate(object sender,DataReceivedEventArgs e) { if(e.Data==null)return;
                    lock(captureGate) { int room=1048576-captured.Length;if(room>0)captured.Append(e.Data,0,Math.Min(room,e.Data.Length)).AppendLine(); }
                    log(e.Data); };
                child.OutputDataReceived+=receive;child.ErrorDataReceived+=receive;
                if(!child.Start())throw new IOException("Could not start the selected Python interpreter.");
                child.BeginOutputReadLine();child.BeginErrorReadLine();child.WaitForExit();
                transcript=captured.ToString();return child.ExitCode;
            }
        }
    }
    internal static class Program {
        [STAThread]
        static void Main() {
            Application.EnableVisualStyles();Application.SetCompatibleTextRenderingDefault(false);
            try { Application.Run(new SetupWindow()); }
            catch(Exception error) { MessageBox.Show("Setup could not start.\r\n"+error.Message,"Enhanced Setup",MessageBoxButtons.OK,MessageBoxIcon.Error); }
        }
    }
}
