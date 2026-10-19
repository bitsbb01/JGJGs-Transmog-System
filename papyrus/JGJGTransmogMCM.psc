Scriptname JGJGTransmogMCM extends SKI_ConfigBase
; Catalogue test MCM. The build generates a Start Game Enabled quest plugin
; and attaches this script plus SkyUI's player-load alias.
; Appearance changes and 3D preview are not implemented in this milestone.
Int rebuildOID
Int browseOID
Int nextOID
Int previousOID
Int pluginOID
Int pageIndex = 0
String filterPlugin = ""
Int menuMode = 0
Event OnConfigInit()
    ModName = "JGJGs Transmog System"
    Pages = new String[2]
    Pages[0] = "Catalogue prototype"
    Pages[1] = "Development status"
EndEvent
Event OnPageReset(String page)
    SetCursorFillMode(TOP_TO_BOTTOM)
    If page == "Development status"
        AddHeaderOption("Not a playable release")
        AddTextOption("Appearance application", "In development", OPTION_FLAG_DISABLED)
        AddTextOption("3D comparison preview", "In development", OPTION_FLAG_DISABLED)
        AddTextOption("Preset persistence", "In development", OPTION_FLAG_DISABLED)
        Return
    EndIf
    AddHeaderOption("Compatible appearance catalogue")
    AddTextOption("Cached items", JGJGTransmogNative.CatalogueCount() as String, OPTION_FLAG_DISABLED)
    rebuildOID = AddTextOption("Build / refresh catalogue", "Select")
    pluginOID = AddMenuOption("Filter by mod", filterPlugin)
    browseOID = AddMenuOption("Right-hand weapon appearances", "Browse")
    previousOID = AddTextOption("Previous page", "Select")
    nextOID = AddTextOption("Next page", "Select")
    AddTextOption("Page requested", pageIndex + 1, OPTION_FLAG_DISABLED)
EndEvent
Event OnOptionSelect(Int option)
    If option == rebuildOID
        If JGJGTransmogNative.RequestCatalogue()
            ShowMessage("Catalogue requested. The list is built in the background; reopen this page when it finishes. This test build does not change equipment.", False)
        Else
            ShowMessage("Catalogue is already building.", False)
        EndIf
    ElseIf option == previousOID
        If pageIndex > 0
            pageIndex -= 1
        EndIf
    ElseIf option == nextOID
        pageIndex += 1
    EndIf
    ForcePageReset()
EndEvent
Event OnOptionMenuOpen(Int option)
    If option == pluginOID
        menuMode = 1
        Form target = Game.GetPlayer().GetEquippedWeapon()
        String[] mods = JGJGTransmogNative.QueryPlugins(target)
        String[] options = new String[mods.Length + 1]
        options[0] = "All mods"
        Int i = 0
        While i < mods.Length
            options[i + 1] = mods[i]
            i += 1
        EndWhile
        SetMenuDialogOptions(options)
        SetMenuDialogStartIndex(0)
        SetMenuDialogDefaultIndex(0)
    ElseIf option == browseOID
        menuMode = 2
        String[] names = JGJGTransmogNative.QueryNames(Game.GetPlayer().GetEquippedWeapon(), "", filterPlugin, pageIndex)
        If names.Length == 0
            names = new String[1]
            names[0] = "No compatible results; build the catalogue first"
        EndIf
        SetMenuDialogOptions(names)
        SetMenuDialogStartIndex(0)
        SetMenuDialogDefaultIndex(0)
    EndIf
EndEvent
Event OnOptionMenuAccept(Int option, Int index)
    If option == pluginOID && menuMode == 1
        If index == 0
            filterPlugin = ""
        Else
            String[] mods = JGJGTransmogNative.QueryPlugins(Game.GetPlayer().GetEquippedWeapon())
            If index <= mods.Length
                filterPlugin = mods[index - 1]
            EndIf
        EndIf
        pageIndex = 0
        ForcePageReset()
    ElseIf option == browseOID && menuMode == 2
        ShowMessage("Catalogue inspection only. Apply and 3D preview are not available in this development build.", False)
    EndIf
EndEvent
