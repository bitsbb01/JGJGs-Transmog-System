Scriptname JGJGTransmogMCM extends SKI_ConfigBase
; Catalogue test MCM. The build generates a Start Game Enabled quest plugin
; and attaches this script plus SkyUI's player-load alias.
; Appearance changes and 3D preview are not implemented in this milestone.
Int rebuildOID
Int browseOID
Int nextOID
Int previousOID
Int searchOID
Int pageIndex = 0
String searchText = ""
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
    searchOID = AddInputOption("Search name", searchText)
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
Event OnOptionInputOpen(Int option)
    If option == searchOID
        SetInputDialogStartText(searchText)
    EndIf
EndEvent
Event OnOptionInputAccept(Int option, String value)
    If option == searchOID
        searchText = value
        pageIndex = 0
        ForcePageReset()
    EndIf
EndEvent
Event OnOptionMenuOpen(Int option)
    If option == browseOID
        String[] names = JGJGTransmogNative.QueryNames(Game.GetPlayer().GetEquippedWeapon(), searchText, "", pageIndex)
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
    If option == browseOID
        ShowMessage("Catalogue inspection only. Apply and 3D preview are not available in this development build.", False)
    EndIf
EndEvent
