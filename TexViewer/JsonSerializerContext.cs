using System.Collections.Generic;
using System.Text.Json.Serialization;

namespace TexViewer
{
    [JsonSerializable(typeof(List<PluginManager.PluginOrder>))]
    [JsonSourceGenerationOptions(PropertyNamingPolicy = JsonKnownNamingPolicy.CamelCase)]
    internal partial class PluginOrderJsonContext : JsonSerializerContext
    {
    }

    [JsonSerializable(typeof(Preference))]
    [JsonSourceGenerationOptions(PropertyNamingPolicy = JsonKnownNamingPolicy.CamelCase)]
    internal partial class PreferenceJsonContext : JsonSerializerContext
    {
    }

}
