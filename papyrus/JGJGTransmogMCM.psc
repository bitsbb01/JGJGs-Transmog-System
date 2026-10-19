Scriptname JGJGTransmogMCM extends SKI_ConfigBase
; Armor appearance test interface. The selected form is used only by the
; player's armor render hook; inventory and equipment are left untouched.
Int rebuildOID
Int targetOID
Int pluginOID
Int browseOID
Int applyOID
Int clearOID
Int previewOID
Int cancelPreviewOID
Int nextOID
Int previousOID
Int searchOID
Int pageIndex = 0
Int menuMode = 0
String filterPlugin = ""
String filterLabel = "All mods"
String searchFilter = ""
String selectedName = "None"
String targetLabel = "Select worn armor"
Form targetArmor
Form selectedAppearance

Event OnConfigInit()
    ModName = "JGJGs Transmog System"
    Pages = new String[2]
    Pages[0] = "Armor appearances"
    Pages[1] = "Development status"
EndEvent

Event OnPageReset(String page)
    SetCursorFillMode(TOP_TO_BOTTOM)
    If page == "Development status"
        AddHeaderOption("Armor test build")
        AddTextOption("Armor appearance changes", "Test build")
        AddTextOption("Apply / restore", "Available")
        AddTextOption("Save / load armor changes", "Implemented; test in game")
        AddTextOption("Temporary appearance preview", "On character")
        AddHeaderOption("Still in development")
        AddTextOption("Weapon appearances", "Not included", OPTION_FLAG_DISABLED)
        AddTextOption("Side-by-side 3D viewer", "Not included", OPTION_FLAG_DISABLED)
        AddTextOption("Favourite outfits", "Not included", OPTION_FLAG_DISABLED)
        Return
    EndIf

    AddHeaderOption("Player armor appearance")
    AddTextOption("Cached catalogue items", JGJGTransmogNative.CatalogueCount() as String, OPTION_FLAG_DISABLED)
    rebuildOID = AddTextOption("Build / refresh catalogue", "Select")
    targetOID = AddMenuOption("Worn armor to change", targetLabel)
    pluginOID = AddMenuOption("Filter by mod", filterLabel)
    searchOID = AddInputOption("Search armor names", searchFilter)
    browseOID = AddMenuOption("Choose appearance", selectedName)
    applyOID = AddTextOption("Apply appearance", "Select")
    previewOID = AddTextOption("Preview selected appearance", "Temporary")
    cancelPreviewOID = AddTextOption("Cancel temporary preview", "Select")
    clearOID = AddTextOption("Restore original appearance", "Select")
    previousOID = AddTextOption("Previous page", "Select")
    nextOID = AddTextOption("Next page", "Select")
    Int pageCount = 1
    If targetArmor
        pageCount = JGJGTransmogNative.QueryPageCount(targetArmor, searchFilter, filterPlugin)
        If pageCount < 1
            pageCount = 1
        EndIf
    EndIf
    AddTextOption("Catalogue page", (pageIndex + 1) as String + " / " + pageCount as String, OPTION_FLAG_DISABLED)
EndEvent

Event OnOptionSelect(Int option)
    If option == rebuildOID
        If JGJGTransmogNative.RequestCatalogue()
            ShowMessage("Catalogue requested. Reopen this page when it finishes.", False)
        Else
            ShowMessage("Catalogue is already building.", False)
        EndIf
    ElseIf option == previousOID
        If pageIndex > 0
            pageIndex -= 1
        EndIf
    ElseIf option == nextOID
        If targetArmor
            Int pageCount = JGJGTransmogNative.QueryPageCount(targetArmor, searchFilter, filterPlugin)
            If pageIndex + 1 < pageCount
                pageIndex += 1
            EndIf
        EndIf
    ElseIf option == applyOID
        If !targetArmor || !selectedAppearance
            ShowMessage("Choose a worn armor item and an appearance first.", False)
        ElseIf JGJGTransmogNative.SetArmorAppearance(targetArmor, selectedAppearance)
            ShowMessage("Armor appearance applied. Your equipped item and its stats were not changed.", False)
        Else
            ShowMessage("Could not apply that appearance. Choose armor with matching body slots.", False)
        EndIf
    ElseIf option == previewOID
        If !targetArmor || !selectedAppearance
            ShowMessage("Choose a worn armor item and an appearance first.", False)
        ElseIf JGJGTransmogNative.PreviewArmorAppearance(targetArmor, selectedAppearance)
            ShowMessage("Temporary preview is active on your character. Select Apply to keep it, or Cancel temporary preview to restore the saved appearance.", False)
        Else
            ShowMessage("Could not preview that appearance. Choose armor with matching body slots.", False)
        EndIf
    ElseIf option == cancelPreviewOID
        If targetArmor && JGJGTransmogNative.CancelArmorPreview(targetArmor)
            ShowMessage("Temporary preview cancelled.", False)
        Else
            ShowMessage("No temporary preview is active for this item.", False)
        EndIf
    ElseIf option == clearOID
        If targetArmor && JGJGTransmogNative.ClearArmorAppearance(targetArmor)
            selectedAppearance = None
            selectedName = "None"
            ShowMessage("Original armor appearance restored.", False)
        Else
            ShowMessage("No appearance override was set for that armor.", False)
        EndIf
    EndIf
    ForcePageReset()
EndEvent

Event OnOptionInputAccept(Int option, String value)
    If option == searchOID
        searchFilter = value
        pageIndex = 0
        ForcePageReset()
    EndIf
EndEvent

Event OnOptionMenuOpen(Int option)
    If option == targetOID
        menuMode = 1
        String[] names = JGJGTransmogNative.GetPlayerWornArmorNames()
        If names.Length == 0
            names = new String[1]
            names[0] = "No armor currently worn"
        EndIf
        SetMenuDialogOptions(names)
        SetMenuDialogStartIndex(0)
        SetMenuDialogDefaultIndex(0)
    ElseIf option == pluginOID && targetArmor
        menuMode = 2
        SetMenuDialogOptions(JGJGTransmogNative.QueryPlugins(targetArmor))
        SetMenuDialogStartIndex(0)
        SetMenuDialogDefaultIndex(0)
    ElseIf option == browseOID && targetArmor
        menuMode = 3
        String[] names = JGJGTransmogNative.QueryNames(targetArmor, searchFilter, filterPlugin, pageIndex)
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
    If option == targetOID && menuMode == 1
        Form[] armors = JGJGTransmogNative.GetPlayerWornArmors()
        String[] names = JGJGTransmogNative.GetPlayerWornArmorNames()
        If index < armors.Length
            If targetArmor
                JGJGTransmogNative.CancelArmorPreview(targetArmor)
            EndIf
            targetArmor = armors[index]
            If index < names.Length
                targetLabel = names[index]
            EndIf
            pageIndex = 0
            selectedAppearance = None
            selectedName = "None"
        EndIf
    ElseIf option == pluginOID && menuMode == 2
        If index == 0
            filterPlugin = ""
            filterLabel = "All mods"
        Else
            String[] mods = JGJGTransmogNative.QueryPlugins(targetArmor)
            If index < mods.Length
                filterPlugin = mods[index]
                filterLabel = filterPlugin
            EndIf
        EndIf
        pageIndex = 0
    ElseIf option == browseOID && menuMode == 3 && targetArmor
        Form[] forms = JGJGTransmogNative.QueryForms(targetArmor, searchFilter, filterPlugin, pageIndex)
        String[] names = JGJGTransmogNative.QueryNames(targetArmor, searchFilter, filterPlugin, pageIndex)
        If index < forms.Length
            JGJGTransmogNative.CancelArmorPreview(targetArmor)
            selectedAppearance = forms[index]
            If index < names.Length
                selectedName = names[index]
            EndIf
        EndIf
    EndIf
    ForcePageReset()
EndEvent
