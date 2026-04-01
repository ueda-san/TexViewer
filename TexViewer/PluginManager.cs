using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Reflection;
using System.Text.Json;
using TexViewerPlugin;

namespace TexViewer
{
    //https://csharpindepth.com/articles/singleton
    public sealed class PluginManager
    {
        public class PluginInfo {
            //public string Name;
            public bool Enabled = true;
            public ITexViewerPlugin Plugin;
            public string[] SupportedExtensions;
            public bool AllowWildcard;
            public bool CanLoadFile = false;
            public bool CanLoadByte = false;
            public bool CanSaveByte = false;

            public bool Found { get; set; }

            public PluginInfo(ITexViewerPlugin p){
                Plugin = p;
                SupportedExtensions = p.SupportedExtensions.Split(",", System.StringSplitOptions.RemoveEmptyEntries).Select(elem => "." + elem).ToArray();
                if (SupportedExtensions.Contains(".*")) {
                    AllowWildcard = true;
                    SupportedExtensions = SupportedExtensions.Where(val => val != ".*").ToArray();
                }else{
                    AllowWildcard = false;
                }
            }
        }
        List<PluginInfo> pluginList = new();

        public class PluginOrder { // for save
            public required string Name { get; set; }
            public bool Enabled { get; set; } = true;
        }

        private static readonly Lazy<PluginManager> lazy =
            new Lazy<PluginManager>(() => new PluginManager());
        public static PluginManager Instance { get { return lazy.Value; } }

        private PluginManager(){
            LoadPlugins();
        }

        //------------------------------------------------------------------------------
        void LoadPlugins()
        {
            string? dir = Path.GetDirectoryName(Assembly.GetEntryAssembly()?.Location);
            if (!string.IsNullOrEmpty(dir)) {
                string path = Path.Combine(dir, "Plugins");
                if (Directory.Exists(path)){
                    var dlls = Directory.GetFiles(path, "*.dll");
                    foreach(string dll in dlls){
                        if (dll.EndsWith("Lib.dll")) continue;
                        var plugin = LoadPlugin(dll, out bool loadFile, out bool loadByte, out bool saveByte);
                        //var plugin = LoadPlugin3(dll, out implPath, out implByte);
                        if (plugin != null){
                            if(plugin.APIVersion == 1 && (loadFile || loadByte)){
                                PluginInfo info = new(plugin) {
                                    CanLoadFile = loadFile,
                                    CanLoadByte = loadByte,
                                    CanSaveByte = saveByte,
                                };
                                pluginList.Add(info);
                            }
                        }
                    }
                }
            }

            MagickLoader.MagickLoader magickLoader = new();
            PluginInfo minfo = new(magickLoader);
            pluginList.Add(minfo);

            if (!LoadOrder()){
                pluginList.Sort((a,b)=>(a.Plugin.DefaultPriority - b.Plugin.DefaultPriority));
            }

        }

        public static ITexViewerPlugin? LoadPlugin(string dllPath, out bool canLoadFile, out bool canLoadByte, out bool canSaveByte)
        {
            canLoadFile = false;
            canLoadByte = false;
            canSaveByte = false;

            AssemblyName asmName;
            try {
                asmName = AssemblyName.GetAssemblyName(dllPath);
            } catch (BadImageFormatException) {
                return null;
            } catch (Exception) {
                return null;
            }

            PluginLoadContext loadContext = new PluginLoadContext(dllPath);
            Assembly asm;
            try {
                asm = loadContext.LoadFromAssemblyName(asmName);
            } catch (Exception ex) {
                Debug.WriteLine(ex);
                return null;
            }

            Type[] types;// = null;
            try {
                types = asm.GetTypes();
            } catch (ReflectionTypeLoadException ex) {
                foreach (var loaderEx in ex.LoaderExceptions) {
                    Trace.WriteLine(loaderEx);
                }
                return null;
            } catch (Exception ex) {
                Trace.WriteLine(ex);
                return null;
            }

            foreach (Type type in types) {
                //Trace.WriteLine($"{type.Name} = IsInterface:{type.IsInterface} IsClass:{type.IsClass} IsPublic:{type.IsPublic}");
                if (typeof(ITexViewerPlugin).IsAssignableFrom(type)) {
                    MethodInfo[] methods = type.GetMethods(BindingFlags.Public | BindingFlags.Instance);
                    foreach (var method in methods) {
                        if (method.Name == "TryLoad"){
                            ParameterInfo[] arr = method.GetParameters();
                            if (arr.Length == 3){
                                if (arr[0].ParameterType.Name == "String" &&
                                    arr[1].ParameterType.Name == "TexInfo&" &&
                                    arr[2].ParameterType.Name == "UInt32") canLoadFile = true;
                                if (arr[0].ParameterType.Name == "Byte[]" &&
                                    arr[1].ParameterType.Name == "TexInfo&" &&
                                    arr[2].ParameterType.Name == "UInt32") canLoadByte = true;
                            }
                        }else if (method.Name == "TrySave"){
                            ParameterInfo[] arr = method.GetParameters();
                            if (arr.Length == 3){
                                if (arr[0].ParameterType.Name == "TexInfo" &&
                                    arr[1].ParameterType.Name == "String" &&
                                    arr[2].ParameterType.Name == "UInt32") canSaveByte = true;
                            }
                        }
                    }
                    return Activator.CreateInstance(type) as ITexViewerPlugin;
                }
            }

            //string availableTypes = string.Join(",", asm.GetTypes().Select(t => t.FullName));
            Trace.WriteLine($"Can't find ITexViewerPlugin in {asm} from {asm.Location}.");
            return null;
        }

        //------------------------------------------------------------------------------
        public void UpdateOrder(List<string> order)
        {
            List<PluginInfo> newList = [];
            foreach(var name in order) {
                foreach(var p in pluginList) {
                    if (p.Plugin.PluginName == name){
                        newList.Add(p);
                    }
                }
            }
            pluginList = newList;
        }

        const string SaveDataKey = "PluginOrder";
        public void SaveOrder(List<string>? order = null)
        {
            if (order != null){
                UpdateOrder(order);
            }

            List<PluginOrder> pluginOrder = [];
            foreach(var p in pluginList) {
                pluginOrder.Add(new PluginOrder(){Name=p.Plugin.PluginName, Enabled=p.Enabled});
            }

            var localSettings = Windows.Storage.ApplicationData.Current.LocalSettings;
            try {
                string json = JsonSerializer.Serialize(pluginOrder, PluginOrderJsonContext.Default.ListPluginOrder);
                localSettings.Values[SaveDataKey] = json;
            }catch(Exception ex){
                Trace.WriteLine(ex.Message);
            }
        }

        public bool LoadOrder()
        {
            var localSettings = Windows.Storage.ApplicationData.Current.LocalSettings;
            Object x = localSettings.Values[SaveDataKey];
            if (x != null){
                try {
                    var pluginOrder = JsonSerializer.Deserialize((string)x, PluginOrderJsonContext.Default.ListPluginOrder);
                    if (pluginOrder == null) return false;
                    List<PluginInfo> newList = [];
                    foreach(var p in pluginList) { p.Found = false; }
                    foreach(var item in pluginOrder) {
                        foreach(var p in pluginList) {
                            if (p.Plugin.PluginName == item.Name){
                                p.Enabled = item.Enabled;
                                newList.Add(p);
                                p.Found = true;
                                break;
                            }
                        }
                    }
                    foreach(var p in pluginList) {
                        if (!p.Found) newList.Insert(0, p);
                    }
                    pluginList = newList;
                    return true;
                }catch(Exception ex){
                    Trace.WriteLine(ex.Message);
                }
            }
            return false;
        }



        //------------------------------------------------------------------------------
        public List<string> GetPluginList(bool withDisabled = false)
        {
            List<string> ret = [];
            foreach(var p in pluginList) {
                if (p.Enabled || withDisabled) ret.Add(p.Plugin.PluginName);
            }
            return ret;
        }

        public List<string> GetPreferredList(string ext, bool withDisabled=false)
        {
            ext = ext.ToLower();
            List<string> preferredList = [];

            foreach(var p in pluginList) {
                if ((p.Enabled)&&(p.SupportedExtensions.Contains(ext) || p.AllowWildcard)) {
                    preferredList.Add(p.Plugin.PluginName);
                }
            }
            if (withDisabled){
                foreach(var p in pluginList) {
                    if (!p.Enabled){
                        preferredList.Add(p.Plugin.PluginName);
                    }
                }
            }
            return preferredList;
        }

        public void SetPluginEnabled(string name, bool enable)
        {
            foreach(var p in pluginList) {
                if (p.Plugin.PluginName.Equals(name)) p.Enabled = enable;
            }
        }

        public PluginInfo? GetPluginInfo(string name)
        {
            foreach(var p in pluginList) {
                if (p.Plugin.PluginName.Equals(name)) return p;
            }
            return null;
        }

        public ITexViewerPlugin? GetPlugin(string name)
        {
            foreach(var p in pluginList) {
                if (p.Plugin.PluginName.Equals(name)) return p.Plugin;
            }
            return null;
        }

    }
}
