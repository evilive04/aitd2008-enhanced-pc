using System;
using System.Globalization;
using System.Linq;
using System.Windows;
using System.Windows.Automation;
using System.Windows.Controls;
using System.Windows.Media;
namespace EnhancedLauncher {
 public sealed partial class LauncherWindow {
  void Home(){
   var hero=new StackPanel{Width=450,HorizontalAlignment=HorizontalAlignment.Left,VerticalAlignment=VerticalAlignment.Top,Margin=new Thickness(52,47,0,0)};
   hero.Children.Add(new Image{Source=ResourceImage("Launcher.Logo"),Width=440,Stretch=Stretch.Uniform,HorizontalAlignment=HorizontalAlignment.Left});
   var edition=Text("E N H A N C E D   E D I T I O N",12,"#D0B786");edition.Margin=new Thickness(4,18,0,0);hero.Children.Add(edition);body.Children.Add(hero);
   var controls=new StackPanel();controls.Children.Add(Text("DISPLAY",11,"#D8BE91"));
   var row=new Grid{Margin=new Thickness(0,14,0,0)};row.ColumnDefinitions.Add(new ColumnDefinition{Width=new GridLength(1,GridUnitType.Star)});row.ColumnDefinitions.Add(new ColumnDefinition{Width=new GridLength(180)});
   resolution=new ComboBox{Margin=new Thickness(0,0,12,0)};AutomationProperties.SetName(resolution,"Resolution");FillResolutions();
   displayMode=new ComboBox();displayMode.Items.Add(new Choice("Borderless","borderless"));displayMode.Items.Add(new Choice("Fullscreen","fullscreen"));displayMode.SelectedIndex=Store.Borderless?0:1;AutomationProperties.SetName(displayMode,"Display mode");
   displayMode.SelectionChanged+=delegate{Store.Borderless=displayMode.SelectedIndex==0;FillResolutions();};
   ActiveControls["Display/Mode"]=displayMode;ActiveControls["Display/Resolution"]=resolution;
   row.Children.Add(resolution);Grid.SetColumn(displayMode,1);row.Children.Add(displayMode);controls.Children.Add(row);
   var help=Text("Borderless uses your monitor’s native resolution. Choose Fullscreen to select a different resolution.",11,"#B0B2AD");help.Margin=new Thickness(0,12,0,0);controls.Children.Add(help);
   var card=Panel(controls,new Thickness(52,0,0,136));card.Width=450;card.HorizontalAlignment=HorizontalAlignment.Left;card.VerticalAlignment=VerticalAlignment.Bottom;body.Children.Add(card);
   var bottom=new Grid{Margin=new Thickness(52,0,52,46),VerticalAlignment=VerticalAlignment.Bottom};bottom.ColumnDefinitions.Add(new ColumnDefinition{Width=new GridLength(255)});bottom.ColumnDefinitions.Add(new ColumnDefinition{Width=new GridLength(180)});bottom.ColumnDefinitions.Add(new ColumnDefinition());
   PlayButton=Button("PLAY  ›",Play,true);PlayButton.Height=58;PlayButton.FontSize=17;bottom.Children.Add(PlayButton);
   var options=Button("Settings",delegate{ShowPage("Graphics");},false);options.Margin=new Thickness(14,0,0,0);options.Height=58;Grid.SetColumn(options,1);bottom.Children.Add(options);
   var logs=Button("Crash logs",OpenLogs,false);logs.HorizontalAlignment=HorizontalAlignment.Right;logs.VerticalAlignment=VerticalAlignment.Bottom;logs.Padding=new Thickness(12,8,12,8);logs.FontSize=11;logs.BorderThickness=new Thickness(0);logs.Background=Brush("#700A1013");Grid.SetColumn(logs,2);bottom.Children.Add(logs);
   body.Children.Add(bottom);status.Margin=new Thickness(55,0,52,16);status.VerticalAlignment=VerticalAlignment.Bottom;status.FontSize=12;status.Foreground=Brush("#C0B9A9");body.Children.Add(status);Poll();
  }
  void FillResolutions(){
   if(resolution==null)return;resolution.SelectionChanged-=ResolutionChanged;resolution.Items.Clear();
   var modes=Native.Modes();if(!modes.Contains(Store.SelectedResolution))modes.Insert(0,Store.SelectedResolution);
   if(Store.Borderless){resolution.Items.Add(Native.Desktop()+"  ·  monitor");resolution.SelectedIndex=0;resolution.IsEnabled=false;}
   else {foreach(var m in modes)resolution.Items.Add(m);resolution.SelectedItem=modes.FirstOrDefault(m=>m.Equals(Store.SelectedResolution));resolution.IsEnabled=true;}
   resolution.SelectionChanged+=ResolutionChanged;
  }
  void ResolutionChanged(object sender,SelectionChangedEventArgs e){var r=resolution.SelectedItem as Resolution;if(!Store.Borderless&&r!=null)Store.SelectedResolution=r;}
  void OptionsPage(string group){
   var sheet=new Grid();sheet.ColumnDefinitions.Add(new ColumnDefinition{Width=new GridLength(180)});sheet.ColumnDefinitions.Add(new ColumnDefinition{Width=new GridLength(1,GridUnitType.Star)});
   sheet.RowDefinitions.Add(new RowDefinition{Height=new GridLength(1,GridUnitType.Star)});sheet.RowDefinitions.Add(new RowDefinition{Height=GridLength.Auto});
   var rail=new StackPanel{Margin=new Thickness(0,6,22,0)};
   rail.Children.Add(new Image{Source=ResourceImage("Launcher.Logo"),Width=143,Stretch=Stretch.Uniform,Margin=new Thickness(0,0,0,32)});
   foreach(var n in new[]{"Graphics","Controls","Gameplay & UI","Audio","Launcher"}){
    string captured=n;var b=Button(n,delegate{ShowPage(captured);},false);b.Margin=new Thickness(0,0,0,8);
    if(n==group){b.Background=Brush("#3DDCB36F");b.BorderBrush=Brush("#CCB58B");b.Foreground=Brush("#F3D4A0");}rail.Children.Add(b);
   }
   var back=Button("‹  Back",delegate{ShowPage("Home");},false);back.Margin=new Thickness(0,30,0,0);rail.Children.Add(back);sheet.Children.Add(rail);
   var content=new StackPanel{Margin=new Thickness(12,0,18,12)};content.Children.Add(Text(group,29));
   var sub=Text("Settings take effect the next time you start the game.",12,"#B2B8B5");sub.Margin=new Thickness(0,8,0,22);content.Children.Add(sub);
   foreach(var o in Catalog.All.Where(o=>o.Group==group))content.Children.Add(OptionRow(o));
   if(group=="Audio"){var extra=Text("Automatic uses the game’s native audio device selection. Choose Stereo / 2.1 if voices behind you are missing on a stereo system.",12,"#B6BBAF");extra.Margin=new Thickness(0,18,0,10);content.Children.Add(extra);}
   if(group=="Launcher"){
    var logs=Button("Open crash logs",OpenLogs,false);logs.HorizontalAlignment=HorizontalAlignment.Left;logs.Margin=new Thickness(0,22,0,0);content.Children.Add(logs);
    var folder=Button("Open game folder",delegate{OpenFolder(gameRoot);},false);folder.HorizontalAlignment=HorizontalAlignment.Left;folder.Margin=new Thickness(0,12,0,0);content.Children.Add(folder);
   }
   var scroll=new ScrollViewer{Content=content,VerticalScrollBarVisibility=ScrollBarVisibility.Auto,HorizontalScrollBarVisibility=ScrollBarVisibility.Disabled};Grid.SetColumn(scroll,1);sheet.Children.Add(scroll);
   var footer=new Grid{Margin=new Thickness(192,18,18,0)};footer.ColumnDefinitions.Add(new ColumnDefinition());footer.ColumnDefinitions.Add(new ColumnDefinition{Width=GridLength.Auto});footer.ColumnDefinitions.Add(new ColumnDefinition{Width=GridLength.Auto});
   var discard=Button("Discard changes",delegate{try{Store.Reload();note="Changes discarded.";ShowPage(page);}catch(Exception ex){Error(ex);}},false);discard.Margin=new Thickness(0,0,12,0);Grid.SetColumn(discard,1);footer.Children.Add(discard);
   var save=Button("Save settings",delegate{if(Save())ShowPage(page);},true);saveButtons.Add(save);Grid.SetColumn(save,2);footer.Children.Add(save);Grid.SetRow(footer,1);Grid.SetColumnSpan(footer,2);sheet.Children.Add(footer);
   body.Children.Add(Panel(sheet,new Thickness(30,12,30,52)));
   status.Margin=new Thickness(36,0,30,18);status.VerticalAlignment=VerticalAlignment.Bottom;status.FontSize=12;status.Foreground=Brush("#C2B7A4");body.Children.Add(status);Poll();
  }
  UIElement OptionRow(Option o){
   var row=new Grid{Margin=new Thickness(0,0,0,2)};row.ColumnDefinitions.Add(new ColumnDefinition());row.ColumnDefinitions.Add(new ColumnDefinition{Width=new GridLength(o.Kind=="bool"?60:184)});
   var label=new StackPanel{Margin=new Thickness(0,14,20,14)};label.Children.Add(Text(o.Label,14));var hint=Text(o.Help,11,"#ACB4B2");hint.Margin=new Thickness(0,5,0,0);label.Children.Add(hint);row.Children.Add(label);
   FrameworkElement control;
   if(o.Kind=="bool"){
    var cb=new CheckBox{IsChecked=Store.Draft[o.Id]=="true",HorizontalAlignment=HorizontalAlignment.Right};
    cb.Checked+=delegate{Store.Draft[o.Id]="true";};cb.Unchecked+=delegate{Store.Draft[o.Id]="false";};control=cb;label.MouseLeftButtonUp+=delegate{cb.IsChecked=!(cb.IsChecked??false);};
   } else if(o.Kind=="range"){
    var stack=new StackPanel{VerticalAlignment=VerticalAlignment.Center};var value=Text("",12,"#E8C98E");value.HorizontalAlignment=HorizontalAlignment.Right;
    var slider=new Slider{Minimum=o.Minimum,Maximum=o.Maximum,TickFrequency=o.Step,SmallChange=o.Step,LargeChange=o.Step*2,IsSnapToTickEnabled=true,Value=double.Parse(Store.Draft[o.Id],CultureInfo.InvariantCulture)};
    Action update=delegate{Store.Draft[o.Id]=slider.Value.ToString("0.###",CultureInfo.InvariantCulture);value.Text=slider.Value.ToString(o.Step<1?"0.00":"0",CultureInfo.CurrentCulture)+"×";};
    slider.ValueChanged+=delegate{update();};update();stack.Children.Add(value);stack.Children.Add(slider);control=stack;ActiveControls[o.Id]=slider;AutomationProperties.SetName(slider,o.Label);
   } else {
    var combo=new ComboBox{VerticalAlignment=VerticalAlignment.Center};var choices=o.Kind=="audio"?new[]{new Choice("Automatic","0"),new Choice("Stereo / 2.1","2")}:new[]{new Choice("Unlimited","0"),new Choice("30 FPS","30"),new Choice("60 FPS","60"),new Choice("120 FPS","120")};
    foreach(var choice in choices)combo.Items.Add(choice);combo.SelectedItem=choices.FirstOrDefault(v=>v.Value==Store.Draft[o.Id]);
    combo.SelectionChanged+=delegate{var c=combo.SelectedItem as Choice;if(c!=null)Store.Draft[o.Id]=c.Value;};control=combo;
   }
   AutomationProperties.SetName(control,o.Label);if(!ActiveControls.ContainsKey(o.Id))ActiveControls[o.Id]=control;
   Grid.SetColumn(control,1);row.Children.Add(control);return new Border{BorderBrush=Brush("#344C524F"),BorderThickness=new Thickness(0,0,0,1),Child=row};
  }
 }
}
