using System;
using System.IO;
using System.Linq;
using System.Text;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Media;
using System.Windows.Media.Imaging;
using System.Windows.Threading;
namespace EnhancedLauncher {
 public static class SelfTests {
  static int checks;
  public static void Check(bool condition,string name){if(!condition)throw new Exception(name);checks++;}
  public static byte[] Profile(Resolution r){return Encoding.ASCII.GetBytes("ResolutionWidth = "+r.Width+"\r\nResolutionHeight = "+r.Height+"\r\nNewResolution = 0\r\nPrevResolutionWidth = "+r.Width+"\r\nPrevResolutionHeight = "+r.Height+"\r\n");}
  public static int Run(){
   string root=Path.Combine(AppDomain.CurrentDomain.BaseDirectory,"test-results","model-"+DateTime.UtcNow.ToString("yyyyMMdd-HHmmss"));Directory.CreateDirectory(root);
   try {
    string text="; preserve comment\r\n[Display]\r\nBorderlessFullscreen = true\r\n[Graphics]\r\nSubsurfaceScattering = true\r\nLodQualityScale=1.27\r\n[Mods]\r\nPythonExecutable=C:\\a path\\python.exe\r\nUnknown=unchanged\r\n[Companions]\r\nFollowNormally=false\r\nCatchUpDistance=77\r\n[Audio]\r\nFixRearStereoVoices=false\r\n";
    foreach(var enc in new Encoding[]{new UTF8Encoding(false),new UTF8Encoding(true),new UnicodeEncoding(false,true),new UnicodeEncoding(true,true)}){
     byte[] input=enc.GetPreamble().Concat(enc.GetBytes(text)).ToArray();var doc=new IniDocument(input);Check(Native.Same(doc.Bytes(),input),"encoding and newline round trip");doc.Set("Graphics","SubsurfaceScattering","false");var again=new IniDocument(doc.Bytes());Check(again.Get("Graphics","SubsurfaceScattering","")=="false","edited key");Check(again.Get("Mods","PythonExecutable","")==@"C:\a path\python.exe","unknown path preserved");
    }
    var ini=Path.Combine(root,"aitd-enhanced.ini");var profile=Path.Combine(root,"profile","res.aitd");File.WriteAllText(ini,text,new UTF8Encoding(false));Native.AtomicWrite(profile,Profile(Native.Desktop()));
    var desktop=Native.Desktop();var stale=desktop.Equals(new Resolution(1280,720))?new Resolution(1920,1080):new Resolution(1280,720);
    Native.AtomicWrite(profile,Profile(stale));var staleBytes=File.ReadAllBytes(profile);var firstStart=new SettingsStore(root,profile);
    Check(firstStart.Borderless&&firstStart.Dirty,"borderless detects stale native render dimensions");
    bool blockedRunning=false;try{firstStart.Save(()=>true);}catch(InvalidOperationException){blockedRunning=true;}
    Check(blockedRunning&&Native.Same(staleBytes,File.ReadAllBytes(profile)),"running game prevents native resolution repair");
    string resolutionBackup=firstStart.Save(()=>false);
    Check(Native.Same(staleBytes,File.ReadAllBytes(Path.Combine(resolutionBackup,"res.aitd.before"))),"borderless repair preserves old resolution in backup");
    Check(Native.Same(Profile(desktop),File.ReadAllBytes(profile))&&!firstStart.Dirty,"borderless saves coherent current and previous monitor dimensions");
    Check(firstStart.Save(()=>false)=="","repaired borderless profile is not rewritten");
    var store=new SettingsStore(root,profile);byte[] original=File.ReadAllBytes(ini);Check(!store.Dirty,"opening is clean");Check(store.Save(()=>false)=="","no-op save");Check(Native.Same(original,File.ReadAllBytes(ini)),"no-op is byte identical");
    store.Draft["Graphics/SubsurfaceScattering"]="false";Check(store.Dirty,"toggle dirty");string backup=store.Save(()=>false);Check(Native.Same(original,File.ReadAllBytes(Path.Combine(backup,"aitd-enhanced.ini.before"))),"backup exact");Check(!store.Dirty,"saved clean");Check(new IniDocument(File.ReadAllBytes(ini)).Get("Mods","Unknown","")=="unchanged","unknown key preserved on save");Check(Native.Same(store.InitialProfile,Profile(Native.Desktop())),"graphic toggle leaves resolution alone");
    var hidden=new[]{"Companions/FollowNormally","Companions/CatchUpDistance","Audio/FixRearStereoVoices"};
    foreach(var id in hidden){Check(!Catalog.All.Any(o=>o.Id==id),"removed setting absent from catalog "+id);Check(!store.Draft.ContainsKey(id),"removed setting absent from editable draft "+id);}
    Check(store.Draft["Camera/DisableFixedAngles"]=="false","fixed camera override defaults off");Check(store.Draft["Cutscenes/FastForwardEvents"]=="true","scripted camera fast-forward defaults on");
    store.Draft["Camera/DisableFixedAngles"]="true";store.Draft["Cutscenes/FastForwardEvents"]="false";store.Save(()=>false);
    var newSettings=new SettingsStore(root,profile);Check(newSettings.Draft["Camera/DisableFixedAngles"]=="true"&&newSettings.Draft["Cutscenes/FastForwardEvents"]=="false","new camera settings persist nondefault values");
    store.Draft["Camera/DisableFixedAngles"]="false";store.Draft["Cutscenes/FastForwardEvents"]="true";store.Save(()=>false);newSettings.Reload();
    Check(newSettings.Draft["Camera/DisableFixedAngles"]=="false"&&newSettings.Draft["Cutscenes/FastForwardEvents"]=="true","new camera settings persist restored defaults");
    var preserved=new IniDocument(File.ReadAllBytes(ini));Check(preserved.Get("Companions","FollowNormally","")=="false","hidden follow setting retained");Check(preserved.Get("Companions","CatchUpDistance","")=="77","hidden catch-up distance retained");Check(preserved.Get("Audio","FixRearStereoVoices","")=="false","hidden stereo setting retained");
    foreach(var key in new[]{"SubsurfaceScattering","GTAO"}){var help=Catalog.All.First(o=>o.Key==key).Help;Check(help.IndexOf("Home",StringComparison.OrdinalIgnoreCase)<0&&help.IndexOf("Page Up",StringComparison.OrdinalIgnoreCase)<0&&help.IndexOf("PageUp",StringComparison.OrdinalIgnoreCase)<0,"retired effect shortcuts absent "+key);}
    store.Draft["Graphics/GTAO"]="false";bool rejected=false;try{store.Save(()=>true);}catch(InvalidOperationException){rejected=true;}Check(rejected,"running game blocks save");
    byte[] changed=Encoding.UTF8.GetBytes(text+"; external change\r\n");File.WriteAllBytes(ini,changed);rejected=false;try{store.Save(()=>false);}catch(InvalidOperationException){rejected=true;}Check(rejected&&Native.Same(changed,File.ReadAllBytes(ini)),"external writer protected");store.Reload();
    store.Draft["Graphics/GTAO"]="false";int calls=0;rejected=false;try{store.Save(()=>++calls>1);}catch(InvalidOperationException){rejected=true;}Check(rejected&&Native.Same(changed,File.ReadAllBytes(ini)),"late game launch protected");
    var modes=Native.Modes();Check(modes.Count>0,"supported monitor modes");var alternative=modes.FirstOrDefault(r=>!r.Equals(store.SelectedResolution));
    if(alternative!=null){
     store.Borderless=false;store.SelectedResolution=alternative;byte[] previous=File.ReadAllBytes(profile);FileStream locked=null;rejected=false;
     try{store.Save(()=>false,delegate(string unused){locked=new FileStream(ini,FileMode.Open,FileAccess.Read,FileShare.Read);});}catch(IOException){rejected=true;}finally{if(locked!=null)locked.Dispose();}
     Check(rejected&&Native.Same(previous,File.ReadAllBytes(profile)),"resolution rolled back if INI commit fails");
     store.Save(()=>false);Check(SettingsStore.ParseResolution(File.ReadAllBytes(profile)).Equals(alternative),"fullscreen resolution persistence");Check(new IniDocument(File.ReadAllBytes(ini)).Get("Display","BorderlessFullscreen","")=="false","fullscreen mode persistence");
    }
    rejected=false;try{SettingsStore.ParseResolution(Encoding.ASCII.GetBytes("ResolutionWidth=1920\n"));}catch(InvalidDataException){rejected=true;}Check(rejected,"malformed native profile rejected");
    var lod=Catalog.All.First(o=>o.Key=="LodQualityScale");Check(Catalog.Normalize(lod,"99")=="1.5","native LOD bound");Check(Catalog.Normalize(lod,"NaN")==lod.Default,"non-finite value fallback");
    File.WriteAllText(Path.Combine(root,"result.txt"),"PASS: "+checks+" checks\r\n",Encoding.UTF8);return 0;
   }catch(Exception ex){File.WriteAllText(Path.Combine(root,"result.txt"),"FAIL: "+ex,Encoding.UTF8);return 1;}
  }
 }
 public static class UiTests {
  static void Render(LauncherWindow w,string path){w.UpdateLayout();var image=new RenderTargetBitmap((int)Math.Ceiling(w.ActualWidth),(int)Math.Ceiling(w.ActualHeight),96,96,PixelFormats.Pbgra32);image.Render(w);var png=new PngBitmapEncoder();png.Frames.Add(BitmapFrame.Create(image));using(var stream=File.Create(path))png.Save(stream);}
  public static void Schedule(LauncherWindow w,string root){w.Dispatcher.BeginInvoke(DispatcherPriority.ApplicationIdle,new Action(delegate{
   try{
    byte[] before=File.ReadAllBytes(w.Store.IniPath);Render(w,Path.Combine(root,"home.png"));
    var mode=(ComboBox)w.ActiveControls["Display/Mode"];var resolution=(ComboBox)w.ActiveControls["Display/Resolution"];mode.SelectedIndex=1;SelfTests.Check(!w.Store.Borderless&&resolution.IsEnabled,"fullscreen selection event");mode.SelectedIndex=0;SelfTests.Check(w.Store.Borderless&&!resolution.IsEnabled,"borderless selection event");
    foreach(string page in new[]{"Graphics","Controls","Gameplay & UI","Audio","Launcher"}){w.ShowPage(page);w.UpdateLayout();foreach(var c in w.ActiveControls.Values){c.ApplyTemplate();SelfTests.Check(!double.IsNaN(c.ActualWidth)&&c.ActualWidth>0,"control layout "+page);}Render(w,Path.Combine(root,page+".png"));}
    SelfTests.Check(!w.Store.Dirty,"visiting every page preserves draft");SelfTests.Check(Native.Same(before,File.ReadAllBytes(w.Store.IniPath)),"viewing pages is read only");
    var runtime=w.PrepareRuntime();SelfTests.Check(runtime.FileName==Path.Combine(root,"AITD Runtime.exe")&&runtime.WorkingDirectory==root&&!runtime.UseShellExecute,"backend launch path and working directory");byte[] runtimeBytes=File.ReadAllBytes(runtime.FileName);SelfTests.Check(runtimeBytes.Length>100000&&runtimeBytes[0]==77&&runtimeBytes[1]==90,"embedded native backend extracted");DateTime runtimeStamp=File.GetLastWriteTimeUtc(runtime.FileName);w.PrepareRuntime();SelfTests.Check(File.GetLastWriteTimeUtc(runtime.FileName)==runtimeStamp,"unchanged backend never rewritten");
    w.ShowPage("Graphics");var check=(CheckBox)w.ActiveControls["Graphics/SubsurfaceScattering"];check.IsChecked=!(check.IsChecked??false);((Slider)w.ActiveControls["Graphics/LodQualityScale"]).Value=1.35;
    string expected=w.Store.Draft["Graphics/SubsurfaceScattering"];w.ShowPage("Audio");((ComboBox)w.ActiveControls["Audio/OutputChannels"]).SelectedIndex=1;w.ShowPage("Graphics");SelfTests.Check(((CheckBox)w.ActiveControls["Graphics/SubsurfaceScattering"]).IsChecked==(expected=="true"),"toggle persists across navigation");SelfTests.Check(w.Save(),"UI save");var reload=new SettingsStore(root,w.Store.ProfilePath);SelfTests.Check(reload.Draft["Graphics/SubsurfaceScattering"]==expected&&reload.Draft["Graphics/LodQualityScale"]=="1.35"&&reload.Draft["Audio/OutputChannels"]=="2","UI handlers persisted exact settings");
    w.Width=920;w.Height=620;w.ShowPage("Home");Render(w,Path.Combine(root,"home-small.png"));w.ShowPage("Graphics");Render(w,Path.Combine(root,"options-small.png"));
    File.WriteAllText(Path.Combine(root,"ui-result.txt"),"PASS: all pages, templates, navigation, input handlers, persistence and minimum-size render.");
   }catch(Exception ex){File.WriteAllText(Path.Combine(root,"ui-result.txt"),"FAIL: "+ex);Environment.ExitCode=1;}
   finally{w.Store.Reload();w.Close();}
  }));}
 }
}
