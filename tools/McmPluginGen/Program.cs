using Mutagen.Bethesda;
using Mutagen.Bethesda.Plugins;
using Mutagen.Bethesda.Skyrim;

if (args.Length != 1 || !args[0].EndsWith(".esp", StringComparison.OrdinalIgnoreCase))
{
    Console.Error.WriteLine("usage: McmPluginGen <output.esp>");
    Environment.ExitCode = 2;
    return;
}

// ESP-FE: a light-flagged ESP that does not consume a full plugin slot.
// The MCM quest uses one new record (0x800), inside Skyrim SE/AE's ESL range.
var pluginName = Path.GetFileName(args[0]);
var modKey = ModKey.FromNameAndExtension(pluginName);
var mod = new SkyrimMod(modKey, SkyrimRelease.SkyrimSE)
{
    IsSmallMaster = true,
};

var quest = new Quest(mod.GetNextFormKey(), SkyrimRelease.SkyrimSE)
{
    EditorID = "JGJGTransmogSystemMCMQuest",
    Name = "JGJGs Transmog System",
    Flags = Quest.Flag.StartGameEnabled,
    VirtualMachineAdapter = new QuestAdapter(),
};
quest.VirtualMachineAdapter.Scripts.Add(new ScriptEntry
{
    Name = "JGJGTransmogMCM",
    Flags = ScriptEntry.Flag.Local,
});

// SkyUI calls this alias when a game is loaded so the MCM can register again.
var playerReference = FormKey.Factory("000014:Skyrim.esm");
quest.Aliases.Add(new QuestAlias
{
    ID = 0,
    Name = "PlayerAlias",
    ForcedReference = playerReference.ToNullableLink<IPlacedGetter>(),
});
quest.VirtualMachineAdapter.Aliases.Add(new QuestFragmentAlias
{
    Property = new ScriptObjectProperty
    {
        Object = quest.ToLink<ISkyrimMajorRecordGetter>(),
        Alias = 0,
    },
    Scripts =
    {
        new ScriptEntry
        {
            Name = "SKI_PlayerLoadGameAlias",
            Flags = ScriptEntry.Flag.Local,
        },
    },
});

mod.Quests.Add(quest);
Directory.CreateDirectory(Path.GetDirectoryName(Path.GetFullPath(args[0]))!);
mod.WriteToBinary(args[0]);
Console.WriteLine($"Created ESP-FE {args[0]} with MCM quest {quest.FormKey} and PlayerAlias.");
