using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Globalization;
using System.IO;
using System.Linq;
using System.Reflection;
using System.Windows;
using System.Windows.Automation;
using System.Windows.Controls;
using System.Windows.Markup;
using System.Windows.Media;
using System.Windows.Media.Imaging;
using System.Windows.Shell;
using System.Windows.Threading;
[assembly:System.Runtime.Versioning.TargetFramework(".NETFramework,Version=v4.8")]
[assembly:AssemblyTitle("Alone in the Dark — Enhanced Edition")]
[assembly:AssemblyProduct("Enhanced Edition Launcher")]
[assembly:AssemblyVersion("1.0.0.0")]
namespace EnhancedLauncher {
 public sealed class Choice {
  public string Text,Value;
  public Choice(string text,string value){Text=text;Value=value;}
  public override string ToString(){return Text;}
 }
 public sealed partial class LauncherWindow : Window {
  public readonly SettingsStore Store;
  readonly Grid root=new Grid(),body=new Grid();
  readonly TextBlock status=new TextBlock();
  readonly DispatcherTimer timer=new DispatcherTimer();
  readonly List<Button> saveButtons=new List<Button>();
  public readonly Dictionary<string,FrameworkElement> ActiveControls=new Dictionary<string,FrameworkElement>();
  public Button PlayButton;
  ComboBox resolution,displayMode;
  Process backend;
  bool launching,closingForGame;
  string page="Home",note="";
  readonly string gameRoot;
  readonly bool automaticTest;
  public string CurrentPage {get{return page;}}
  public LauncherWindow(string directory,string profile,bool test=false) {
   automaticTest=test;gameRoot=directory;Store=new SettingsStore(directory,profile);
   Title="Alone in the Dark — Enhanced Edition";
   Width=Math.Min(1100,SystemParameters.WorkArea.Width-40);Height=Math.Min(730,SystemParameters.WorkArea.Height-40);
   MinWidth=920;MinHeight=620;WindowStartupLocation=WindowStartupLocation.CenterScreen;
   WindowStyle=WindowStyle.None;ResizeMode=ResizeMode.CanResize;Background=Brush("#0B0E10");Foreground=Brush("#F2EBDD");FontFamily=new FontFamily("Segoe UI");FontSize=14;ShowActivated=!test;
   WindowChrome.SetWindowChrome(this,new WindowChrome{CaptionHeight=42,ResizeBorderThickness=new Thickness(6),CornerRadius=new CornerRadius(10),GlassFrameThickness=new Thickness(0),UseAeroCaptionButtons=false});
   using(var s=Assembly.GetExecutingAssembly().GetManifestResourceStream("Launcher.Styles"))Resources=(ResourceDictionary)XamlReader.Load(s);
   var photo=new Image{Source=ResourceImage("Launcher.Background"),Stretch=Stretch.UniformToFill};root.Children.Add(photo);
   var veil=new Border{Background=new LinearGradientBrush(new GradientStopCollection{new GradientStop(Color.FromArgb(190,6,10,12),0),new GradientStop(Color.FromArgb(20,6,10,12),.74),new GradientStop(Color.FromArgb(52,6,10,12),1)},0)};root.Children.Add(veil);
   root.RowDefinitions.Add(new RowDefinition{Height=new GridLength(43)});root.RowDefinitions.Add(new RowDefinition{Height=new GridLength(1,GridUnitType.Star)});Grid.SetRowSpan(photo,2);Grid.SetRowSpan(veil,2);
   var header=new Grid{Margin=new Thickness(24,0,10,0)};header.ColumnDefinitions.Add(new ColumnDefinition());header.ColumnDefinitions.Add(new ColumnDefinition{Width=GridLength.Auto});
   var brand=Text("ENHANCED EDITION  /  PC",11,"#B7A98D");brand.VerticalAlignment=VerticalAlignment.Center;header.Children.Add(brand);
   var chrome=new StackPanel{Orientation=Orientation.Horizontal};
   var minimize=Button("—",delegate{WindowState=WindowState.Minimized;},false);minimize.Width=40;minimize.Height=30;minimize.Padding=new Thickness(0);WindowChrome.SetIsHitTestVisibleInChrome(minimize,true);
   var close=Button("×",delegate{Close();},false);close.Width=40;close.Height=30;close.FontSize=21;close.Padding=new Thickness(0);WindowChrome.SetIsHitTestVisibleInChrome(close,true);
   chrome.Children.Add(minimize);chrome.Children.Add(close);Grid.SetColumn(chrome,1);header.Children.Add(chrome);root.Children.Add(header);
   Grid.SetRow(body,1);root.Children.Add(body);Content=root;
   Closing+=delegate(object sender,System.ComponentModel.CancelEventArgs e){
    if(!closingForGame&&Store.Dirty){var result=MessageBox.Show(this,"Save your settings before closing?","Unsaved changes",MessageBoxButton.YesNoCancel,MessageBoxImage.Question);
     if(result==MessageBoxResult.Cancel){e.Cancel=true;return;}if(result==MessageBoxResult.Yes&&!Save())e.Cancel=true;}};
   Closed+=delegate{timer.Stop();};timer.Interval=TimeSpan.FromMilliseconds(600);timer.Tick+=delegate{Poll();};timer.Start();ShowPage("Home");Poll();
  }
  static SolidColorBrush Brush(string color){var b=new SolidColorBrush((Color)ColorConverter.ConvertFromString(color));b.Freeze();return b;}
  static TextBlock Text(string text,double size=14,string color="#F2EBDD"){return new TextBlock{Text=text,FontSize=size,Foreground=Brush(color),TextWrapping=TextWrapping.Wrap};}
  static BitmapImage ResourceImage(string name){using(var s=Assembly.GetExecutingAssembly().GetManifestResourceStream(name)){if(s==null)throw new FileNotFoundException(name);var b=new BitmapImage();b.BeginInit();b.CacheOption=BitmapCacheOption.OnLoad;b.StreamSource=s;b.EndInit();b.Freeze();return b;}}
  Button Button(string text,Action action,bool gold){
   var b=new Button{Content=text};AutomationProperties.SetName(b,text);
   if(gold){b.Background=Brush("#DCB36F");b.Foreground=Brush("#14191A");b.BorderBrush=Brush("#F2D69E");b.FontWeight=FontWeights.SemiBold;}b.Click+=delegate{action();};return b;
  }
  Border Panel(UIElement child,Thickness margin){return new Border{Background=Brush("#E90F1519"),BorderBrush=Brush("#584E493C"),BorderThickness=new Thickness(1),CornerRadius=new CornerRadius(10),Padding=new Thickness(24),Margin=margin,Child=child};}
  public void ShowPage(string value){page=value;body.Children.Clear();saveButtons.Clear();ActiveControls.Clear();resolution=null;if(value=="Home")Home();else OptionsPage(value);}
  public bool Save(){try{var backup=Store.Save(Native.GameRunning);note=backup==""?"Settings are up to date.":"Settings saved.";Poll();return true;}catch(Exception ex){Error(ex);return false;}}
  public ProcessStartInfo PrepareRuntime(){
   var target=Path.Combine(gameRoot,"AITD Runtime.exe");byte[] runtime;
   using(var s=Assembly.GetExecutingAssembly().GetManifestResourceStream("Launcher.Runtime"))using(var mem=new MemoryStream()){s.CopyTo(mem);runtime=mem.ToArray();}
   if(!Native.Same(Native.ReadOptional(target),runtime)){
    if(File.Exists(target)){var folder=Path.Combine(gameRoot,"aitd-enhanced-backups");Directory.CreateDirectory(folder);File.Copy(target,Path.Combine(folder,"runtime-"+DateTime.UtcNow.ToString("yyyyMMdd-HHmmss-fffffff")+".exe"));}
    Native.AtomicWrite(target,runtime);
   }
   return new ProcessStartInfo{FileName=target,WorkingDirectory=gameRoot,UseShellExecute=false};
  }
  public void Play(){
   if(launching||Native.GameRunning()){note="The game is already running.";Poll();return;}if(!Save())return;
   try{
    backend=Process.Start(PrepareRuntime());
    if(backend==null)throw new InvalidOperationException("The game runtime could not be started.");launching=true;note="Preparing and starting the game…";Poll();
   }catch(Exception ex){Error(ex);}
  }
  void Poll(){
   bool running=Native.GameRunning();
   if(launching){
    if(running){launching=false;note="The game is running.";if(Store.Draft["Launcher/CloseOnLaunch"]=="true"){closingForGame=true;Close();return;}}
    else if(backend!=null&&backend.HasExited){launching=false;note="The game did not start. Check the runtime message or crash logs.";}
   }
   if(PlayButton!=null)PlayButton.IsEnabled=!running&&!launching;foreach(var b in saveButtons)b.IsEnabled=!running&&!launching;
   status.Text=running?"The game is running. Close it to save settings.":launching?note:note!=""?note:Store.Dirty?"You have unsaved changes.":"Ready to play.";
  }
  void Error(Exception ex){note=ex.Message;Poll();if(!automaticTest)MessageBox.Show(this,ex.Message,"Enhanced Edition",MessageBoxButton.OK,MessageBoxImage.Warning);}
  static void OpenFolder(string path){if(Directory.Exists(path))Process.Start(new ProcessStartInfo{FileName=path,UseShellExecute=true});}
  void OpenLogs(){var p=Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),"AITD2008Enhanced","crashes");if(Directory.Exists(p))OpenFolder(p);else MessageBox.Show(this,"No crash logs have been saved yet.","Crash logs",MessageBoxButton.OK,MessageBoxImage.Information);}
 }
 public static class Entry {
  [STAThread] public static int Main(string[] args){
   var english=CultureInfo.GetCultureInfo("en-US");CultureInfo.DefaultThreadCurrentCulture=english;CultureInfo.DefaultThreadCurrentUICulture=english;System.Threading.Thread.CurrentThread.CurrentCulture=english;System.Threading.Thread.CurrentThread.CurrentUICulture=english;
   if(args.Contains("--self-test"))return SelfTests.Run();
   var root=AppDomain.CurrentDomain.BaseDirectory;var profile=Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),"Atari","AITD","res.aitd");
   bool test=args.Contains("--ui-test");if(test){int i=Array.IndexOf(args,"--ui-test");if(i+1>=args.Length)return 2;root=Path.GetFullPath(args[i+1]);profile=Path.Combine(root,"test-profile","res.aitd");}
   bool created;using(var mutex=new System.Threading.Mutex(true,@"Local\AITDCustomLauncher-"+Native.Hash(root),out created)){
    if(!created){MessageBox.Show("The launcher is already open.","Enhanced Edition");return 0;}
    try{
     var app=new Application();app.DispatcherUnhandledException+=delegate(object sender,DispatcherUnhandledExceptionEventArgs e){MessageBox.Show(e.Exception.Message,"Enhanced Edition",MessageBoxButton.OK,MessageBoxImage.Error);e.Handled=true;};
     var w=new LauncherWindow(root,profile,test);if(test)w.Loaded+=delegate{UiTests.Schedule(w,root);};return app.Run(w);
    }catch(Exception ex){MessageBox.Show(ex.Message,"Enhanced Edition — launcher",MessageBoxButton.OK,MessageBoxImage.Error);return 1;}
   }
  }
 }
}
