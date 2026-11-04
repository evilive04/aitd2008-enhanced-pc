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
        public string Message, Log, Launcher;
    }
    internal sealed class SetupWindow : Form {
        readonly Color ink=Color.FromArgb(18,21,23), panel=Color.FromArgb(29,33,36);
        readonly Color text=Color.FromArgb(235,230,219), gold=Color.FromArgb(218,174,101);
        readonly string distribution=Path.GetFullPath(AppDomain.CurrentDomain.BaseDirectory);
        readonly TextBox game=new TextBox(), python=new TextBox(), output=new TextBox();
        readonly Label status=new Label(), requirements=new Label();
        readonly Button install=new Button(), close=new Button(), browseGame=new Button(), browsePython=new Button(), openFolder=new Button();
        readonly ProgressBar progress=new ProgressBar();
        bool busy, finished, releaseReady;
        string logPath, installedGame;

        public SetupWindow() {
            Text="Alone in the Dark - Enhanced Setup";ClientSize=new Size(850,690);
            MinimumSize=new Size(780,660);StartPosition=FormStartPosition.CenterScreen;
            BackColor=ink;ForeColor=text;Font=new Font("Segoe UI",10F);AutoScaleMode=AutoScaleMode.Dpi;
            try { Icon=Icon.ExtractAssociatedIcon(Application.ExecutablePath); } catch(ArgumentException) { }
            var root=new TableLayoutPanel { Dock=DockStyle.Fill,Padding=new Padding(28),ColumnCount=1,RowCount=6,BackColor=ink };
            root.RowStyles.Add(new RowStyle(SizeType.Absolute,111));
            root.RowStyles.Add(new RowStyle(SizeType.Absolute,134));
            root.RowStyles.Add(new RowStyle(SizeType.Absolute,74));
            root.RowStyles.Add(new RowStyle(SizeType.Percent,100));
            root.RowStyles.Add(new RowStyle(SizeType.Absolute,60));
            root.RowStyles.Add(new RowStyle(SizeType.Absolute,48));
            Controls.Add(root);
            var header=new Panel { Dock=DockStyle.Fill,BackColor=ink };
            var title=LabelAt("ALONE IN THE DARK",new Rectangle(0,0,760,40));title.Font=new Font("Georgia",24F,FontStyle.Bold);title.ForeColor=gold;
            var subtitle=LabelAt("ENHANCED PC EDITION  /  SETUP",new Rectangle(2,44,760,27));subtitle.Font=new Font("Segoe UI",11F,FontStyle.Bold);
            var detail=LabelAt("An unofficial mod for your original Steam installation.",new Rectangle(2,77,760,27));detail.ForeColor=Color.FromArgb(174,179,179);
            header.Controls.AddRange(new Control[]{title,subtitle,detail});root.Controls.Add(header,0,0);

            var choices=new TableLayoutPanel { Dock=DockStyle.Fill,ColumnCount=3,RowCount=4,BackColor=ink };
            choices.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute,120));choices.ColumnStyles.Add(new ColumnStyle(SizeType.Percent,100));choices.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute,104));
            choices.RowStyles.Add(new RowStyle(SizeType.Absolute,39));choices.RowStyles.Add(new RowStyle(SizeType.Absolute,23));choices.RowStyles.Add(new RowStyle(SizeType.Absolute,39));choices.RowStyles.Add(new RowStyle(SizeType.Absolute,27));
            ConfigureField(game);ConfigureField(python);ConfigureButton(browseGame,"Browse...");ConfigureButton(browsePython,"Browse...");
            choices.Controls.Add(FieldLabel("Game folder"),0,0);choices.Controls.Add(game,1,0);choices.Controls.Add(browseGame,2,0);
            var gameHint=FieldLabel("Select the folder containing the original Alone.exe. Close the game first.");gameHint.Font=new Font("Segoe UI",8.5F);gameHint.ForeColor=Color.Silver;choices.Controls.Add(gameHint,1,1);choices.SetColumnSpan(gameHint,2);
            choices.Controls.Add(FieldLabel("Python"),0,2);choices.Controls.Add(python,1,2);choices.Controls.Add(browsePython,2,2);
            var pythonHint=FieldLabel("64-bit Python 3.12+ prepares the game's UI assets locally.");pythonHint.Font=new Font("Segoe UI",8.5F);pythonHint.ForeColor=Color.Silver;choices.Controls.Add(pythonHint,1,3);choices.SetColumnSpan(pythonHint,2);
            root.Controls.Add(choices,0,1);
            requirements.Dock=DockStyle.Fill;requirements.Text="Required Python packages: Pillow 13.0.0 and NumPy 2.5.4.\r\nSetup never downloads dependencies, changes activation or replaces original game files.";requirements.ForeColor=Color.FromArgb(183,186,181);requirements.Padding=new Padding(0,6,0,0);root.Controls.Add(requirements,0,2);

            output.Dock=DockStyle.Fill;output.Multiline=true;output.ReadOnly=true;output.ScrollBars=ScrollBars.Vertical;
            output.BackColor=panel;output.ForeColor=text;output.BorderStyle=BorderStyle.FixedSingle;output.Font=new Font("Consolas",9F);output.WordWrap=true;root.Controls.Add(output,0,3);
            var statusArea=new TableLayoutPanel { Dock=DockStyle.Fill,ColumnCount=1,RowCount=2,Padding=new Padding(0,8,0,0) };
            statusArea.RowStyles.Add(new RowStyle(SizeType.Percent,100));statusArea.RowStyles.Add(new RowStyle(SizeType.Absolute,8));status.Dock=DockStyle.Fill;status.AutoEllipsis=true;status.Text="Ready to check your selected folders.";progress.Dock=DockStyle.Fill;progress.Style=ProgressBarStyle.Blocks;
            statusArea.Controls.Add(status,0,0);statusArea.Controls.Add(progress,0,1);root.Controls.Add(statusArea,0,4);
            var buttons=new FlowLayoutPanel { Dock=DockStyle.Fill,FlowDirection=FlowDirection.RightToLeft,WrapContents=false,Padding=new Padding(0,5,0,0) };
            ConfigureButton(close,"Close");ConfigureButton(install,"Install mod");ConfigureButton(openFolder,"Open game folder");
            install.BackColor=gold;install.ForeColor=Color.FromArgb(27,25,21);install.Font=new Font(Font,FontStyle.Bold);install.Width=142;openFolder.Width=160;openFolder.Visible=false;
            buttons.Controls.Add(close);buttons.Controls.Add(install);buttons.Controls.Add(openFolder);root.Controls.Add(buttons,0,5);
            browseGame.Click+=BrowseGame;browsePython.Click+=BrowsePython;install.Click+=InstallClicked;close.Click+=delegate { Close(); };
            openFolder.Click+=delegate { if(!String.IsNullOrEmpty(installedGame))Process.Start(new ProcessStartInfo(Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.Windows),"explorer.exe"),WindowsArguments.Quote(installedGame)) { UseShellExecute=false }); };
            FormClosing+=ClosingWhileBusy;
            game.Text=DetectGame();python.Text=DetectPython();
            releaseReady=File.Exists(Path.Combine(distribution,"tools","install_mod.py"))&&File.Exists(Path.Combine(distribution,"PAYLOAD_MANIFEST.json"))&&Directory.Exists(Path.Combine(distribution,"payload"));
            if(!releaseReady){
                install.Enabled=false;status.Text="A packaged Release is required.";status.ForeColor=gold;
                output.Text="This folder does not contain an installable Release payload.\r\n\r\nDownload a packaged Release, or build the Release payload from the source first. Keep this setup executable beside PAYLOAD_MANIFEST.json, the payload folder and the tools folder.\r\n\r\nNo game files have been changed.";
            }else output.Text="Start and activate your original game normally before installing.\r\nChoose a clean game folder. Existing mod files are never overwritten.\r\n\r\nSelect your installed Python interpreter, then choose Install mod.\r\n";
        }
        Label LabelAt(string value,Rectangle bounds) { return new Label { Text=value,Bounds=bounds,AutoEllipsis=true }; }
        Label FieldLabel(string value) { return new Label { Text=value,Dock=DockStyle.Fill,TextAlign=ContentAlignment.MiddleLeft,AutoEllipsis=true }; }
        void ConfigureField(TextBox box) { box.Dock=DockStyle.Fill;box.Margin=new Padding(0,5,10,5);box.BackColor=panel;box.ForeColor=text;box.BorderStyle=BorderStyle.FixedSingle; }
        void ConfigureButton(Button button,string value) { button.Text=value;button.Width=98;button.Height=33;button.Margin=new Padding(6,2,0,2);button.BackColor=panel;button.ForeColor=text;button.FlatStyle=FlatStyle.Flat;button.FlatAppearance.BorderColor=Color.FromArgb(88,83,70);button.UseVisualStyleBackColor=false; }
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
            install.Enabled=!value&&!finished&&releaseReady;close.Enabled=!value;openFolder.Enabled=!value;
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
                gameFolder=Path.GetFullPath(game.Text.Trim());pythonFile=Path.GetFullPath(python.Text.Trim());
                if(!File.Exists(Path.Combine(gameFolder,"Alone.exe")))throw new IOException("Choose the game folder containing the original Alone.exe.");
                if(!File.Exists(pythonFile)||!String.Equals(Path.GetExtension(pythonFile),".exe",StringComparison.OrdinalIgnoreCase))throw new IOException("Choose an installed 64-bit python.exe interpreter.");
            }catch(Exception error) {
                MessageBox.Show(this,error.Message,"Check the selected paths",MessageBoxButtons.OK,MessageBoxIcon.Information);return;
            }
            SetBusy(true);status.ForeColor=text;status.Text="Checking Python and preparing local assets...";output.Clear();
            InstallResult result;
            try { result=await Task.Run(()=>RunInstallation(gameFolder,pythonFile)); }
            catch(Exception error) { result=new InstallResult { Success=false,Message=error.Message,Log=logPath }; }
            finished=result.Success;SetBusy(false);
            if(result.Success){
                installedGame=gameFolder;openFolder.Visible=true;status.Text="Installation complete. Your Enhanced launcher is ready.";status.ForeColor=Color.FromArgb(159,202,161);
                AppendLog("");AppendLog("READY TO PLAY");AppendLog(result.Launcher);AppendLog("Original game files, activation and saved games were preserved.");
                MessageBox.Show(this,"Installation complete.\r\n\r\nStart the mod with:\r\n"+result.Launcher,"Enhanced PC Edition",MessageBoxButtons.OK,MessageBoxIcon.Information);
            }else{
                status.Text="Installation did not complete. See the log below.";status.ForeColor=gold;
                AppendLog("");AppendLog(result.Message);if(!String.IsNullOrEmpty(result.Log))AppendLog("Full log: "+result.Log);
                MessageBox.Show(this,result.Message+(String.IsNullOrEmpty(result.Log)?"":"\r\n\r\nFull log:\r\n"+result.Log),"Setup could not complete",MessageBoxButtons.OK,MessageBoxIcon.Warning);
            }
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
                string probe="import sys,struct,importlib.metadata as m\n"+
                    "def version(name):\n"+
                    " try: return m.version(name)\n"+
                    " except m.PackageNotFoundError: return 'missing'\n"+
                    "print('AITD_SETUP_ENV\\t%d\\t%d\\t%d\\t%s\\t%s' % (sys.version_info[0],sys.version_info[1],struct.calcsize('P')*8,version('Pillow'),version('numpy')))";
                string probeText;int probeExit=RunProcess(pythonFile,new[]{"-E","-u","-X","utf8","-c",probe},log,out probeText);
                string environment=probeText.Split(new[]{'\r','\n'},StringSplitOptions.RemoveEmptyEntries).LastOrDefault(x=>x.StartsWith("AITD_SETUP_ENV\t",StringComparison.Ordinal));
                var fields=environment==null?new string[0]:environment.Split('\t');int major=0,minor=0,bits=0;
                bool valid=probeExit==0&&fields.Length==6&&Int32.TryParse(fields[1],out major)&&Int32.TryParse(fields[2],out minor)&&Int32.TryParse(fields[3],out bits)&&
                    (major>3||(major==3&&minor>=12))&&bits==64&&fields[4]=="13.0.0"&&fields[5]=="2.5.4";
                if(!valid)return new InstallResult { Message="Python requirements were not met. Select 64-bit Python 3.12 or newer with Pillow 13.0.0 and NumPy 2.5.4. See requirements-install.txt in the packaged Release. Setup does not download or install dependencies.",Log=logPath };
                log("Python requirements confirmed. Preparing assets from your own game; this may take a few minutes.");
                string script=Path.Combine(distribution,"tools","install_mod.py"),ignored;
                int exit=RunProcess(pythonFile,new[]{"-E","-u","-X","utf8",script,gameFolder,"--distribution",distribution},log,out ignored);
                if(exit!=0)return new InstallResult { Message="The installer reported an error (exit code "+exit+"). Review the log for the exact cause. Use a clean supported game folder and check requirements-install.txt. The backend preserves pre-existing files and rolls back its own incomplete file copies.",Log=logPath };
                string launcher=Path.Combine(gameFolder,"AITD Enhanced Launcher.exe");
                if(!File.Exists(launcher))return new InstallResult { Message="The installer exited without the expected launcher. Keep the log and inspect the game folder before trying again.",Log=logPath };
                log("Installation complete: "+launcher);return new InstallResult { Success=true,Launcher=launcher,Log=logPath,Message="Installation complete." };
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
