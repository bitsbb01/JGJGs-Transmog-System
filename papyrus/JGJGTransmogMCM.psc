Scriptname JGJGTransmogMCM extends SKI_ConfigBase
; Render-only player armor and weapon appearances. Equipment is never replaced.
Int categoryOID
Int targetOID
Int pluginOID
Int browseOID
Int applyOID
Int clearOID
Int previewOID
Int cancelPreviewOID
Int nextOID
Int previousOID
Int presetOID
Int savePresetOID
Int loadPresetOID
Int pageIndex = 0
Int menuMode = 0
Int selectedPreset = 0
Int weaponMode = 0
String filterPlugin = ""
String filterLabel = "All mods"
String selectedName = "None"
String targetLabel = "Select worn armor"
String presetLabel = "Favorite 1"
Form targetItem
Form selectedAppearance
Form[] menuTargets
String[] menuTargetNames

Event OnConfigInit()
    ModName = "JGJGs Transmog System"
    Pages = new String[2]
    Pages[0] = "Appearances"
    Pages[1] = "Development status"
EndEvent

Event OnPageReset(String page)
    SetCursorFillMode(TOP_TO_BOTTOM)
    If page == "Development status"
        AddHeaderOption("Player appearance system")
        AddTextOption("Armor appearances", "Working in game")
        AddTextOption("Weapon appearances", "Same-type swaps")
        AddTextOption("Save/load favorites", "Armor and weapons")
        AddTextOption("Temporary preview", "On character")
        AddTextOption("Side-by-side 3D viewer", "Still in development", OPTION_FLAG_DISABLED)
        Return
    EndIf

    AddHeaderOption("Player appearance")
    AddTextOption("Cached catalogue items", JGJGTransmogNative.CatalogueCount() as String, OPTION_FLAG_DISABLED)
    If weaponMode == 0
        categoryOID = AddMenuOption("Browse category", "Armor and shields")
    Else
        categoryOID = AddMenuOption("Browse category", "Weapons")
    EndIf
    targetOID = AddMenuOption("Worn item to change", targetLabel)
    pluginOID = AddMenuOption("Filter by mod", filterLabel)
    browseOID = AddMenuOption("Choose appearance", selectedName)
    applyOID = AddTextOption("Apply appearance", "Select")
    previewOID = AddTextOption("Preview selected appearance", "Temporary")
    cancelPreviewOID = AddTextOption("Cancel temporary preview", "Select")
    clearOID = AddTextOption("Restore original appearance", "Select")
    presetOID = AddMenuOption("Outfit favorite", presetLabel)
    savePresetOID = AddTextOption("Save armor and weapons", "Select")
    loadPresetOID = AddTextOption("Load outfit favorite", "Select")
    previousOID = AddTextOption("Previous page", "Select")
    nextOID = AddTextOption("Next page", "Select")
    Int pageCount = 1
    If targetItem
        pageCount = JGJGTransmogNative.QueryPageCount(targetItem, filterPlugin)
        If pageCount < 1
            pageCount = 1
        EndIf
    EndIf
    AddTextOption("Catalogue page", (pageIndex + 1) as String + " / " + pageCount as String, OPTION_FLAG_DISABLED)
EndEvent

Event OnOptionSelect(Int option)
    If option == previousOID
        If pageIndex > 0
            pageIndex -= 1
        EndIf
    ElseIf option == nextOID
        If targetItem
            Int pageCount = JGJGTransmogNative.QueryPageCount(targetItem, filterPlugin)
            If pageIndex + 1 < pageCount
                pageIndex += 1
            EndIf
        EndIf
    ElseIf option == applyOID
        If !targetItem || !selectedAppearance
            ShowMessage("Choose a worn item and an appearance first.", False)
        ElseIf weaponMode == 1
            If JGJGTransmogNative.SetWeaponAppearance(targetItem, selectedAppearance)
                ShowMessage("Weapon appearance applied. The equipped item, enchantment, and stats were not changed.", False)
            Else
                ShowMessage("Could not apply that appearance. Choose a weapon of the same type.", False)
            EndIf
        ElseIf JGJGTransmogNative.SetArmorAppearance(targetItem, selectedAppearance)
            ShowMessage("Armor appearance applied. Your equipped item and its stats were not changed.", False)
        Else
            ShowMessage("Could not apply that appearance. Choose armor with matching body slots.", False)
        EndIf
    ElseIf option == previewOID
        If !targetItem || !selectedAppearance
            ShowMessage("Choose a worn item and an appearance first.", False)
        ElseIf weaponMode == 1
            If JGJGTransmogNative.PreviewWeaponAppearance(targetItem, selectedAppearance)
                ShowMessage("Temporary weapon preview is active. Apply to keep it, or cancel to restore the saved appearance.", False)
            Else
                ShowMessage("Could not preview that appearance. Choose a weapon of the same type.", False)
            EndIf
        ElseIf JGJGTransmogNative.PreviewArmorAppearance(targetItem, selectedAppearance)
            ShowMessage("Temporary armor preview is active. Apply to keep it, or cancel to restore the saved appearance.", False)
        Else
            ShowMessage("Could not preview that appearance. Choose armor with matching body slots.", False)
        EndIf
    ElseIf option == cancelPreviewOID
        If weaponMode == 1 && targetItem && JGJGTransmogNative.CancelWeaponPreview(targetItem)
            ShowMessage("Temporary preview cancelled.", False)
        ElseIf weaponMode == 0 && targetItem && JGJGTransmogNative.CancelArmorPreview(targetItem)
            ShowMessage("Temporary preview cancelled.", False)
        Else
            ShowMessage("No temporary preview is active for this item.", False)
        EndIf
    ElseIf option == clearOID
        If weaponMode == 1 && targetItem && JGJGTransmogNative.ClearWeaponAppearance(targetItem)
            selectedAppearance = None
            selectedName = "None"
            ShowMessage("Original weapon appearance restored.", False)
        ElseIf weaponMode == 0 && targetItem && JGJGTransmogNative.ClearArmorAppearance(targetItem)
            selectedAppearance = None
            selectedName = "None"
            ShowMessage("Original armor appearance restored.", False)
        Else
            ShowMessage("No appearance override was set for that item.", False)
        EndIf
    ElseIf option == savePresetOID
        If ShowMessage("Save the current armor and weapon appearances to " + presetLabel + "?", True)
            If JGJGTransmogNative.SaveArmorPreset(selectedPreset)
                ShowMessage("Saved " + presetLabel + ".", False)
            Else
                ShowMessage("Could not save that favorite slot.", False)
            EndIf
        EndIf
    ElseIf option == loadPresetOID
        If ShowMessage("Load " + presetLabel + "? This replaces current armor and weapon appearance choices.", True)
            If JGJGTransmogNative.LoadArmorPreset(selectedPreset)
                Int skipped = JGJGTransmogNative.GetLastPresetSkipped()
                If skipped > 0
                    ShowMessage("Loaded " + presetLabel + ". Skipped " + skipped as String + " missing or incompatible appearance(s).", False)
                Else
                    ShowMessage("Loaded " + presetLabel + ".", False)
                EndIf
            Else
                ShowMessage("Could not load that favorite slot.", False)
            EndIf
        EndIf
    EndIf
    ForcePageReset()
EndEvent

Event OnOptionMenuOpen(Int option)
    If option == categoryOID
        menuMode = 0
        String[] categories = new String[2]
        categories[0] = "Armor and shields"
        categories[1] = "Weapons"
        SetMenuDialogOptions(categories)
        SetMenuDialogStartIndex(weaponMode)
        SetMenuDialogDefaultIndex(weaponMode)
    ElseIf option == targetOID
        menuMode = 1
        If weaponMode == 1
            Form[] weapons = JGJGTransmogNative.GetPlayerWornWeapons()
            menuTargetNames = JGJGTransmogNative.GetPlayerWornWeaponNames()
            menuTargets = weapons
        Else
            Form[] armors = JGJGTransmogNative.GetPlayerWornArmors()
            menuTargetNames = JGJGTransmogNative.GetPlayerWornArmorNames()
            menuTargets = armors
        EndIf
        If menuTargetNames.Length == 0
            menuTargetNames = new String[1]
            If weaponMode == 1
                menuTargetNames[0] = "No weapon equipped"
            Else
                menuTargetNames[0] = "No armor currently worn"
            EndIf
        EndIf
        SetMenuDialogOptions(menuTargetNames)
        SetMenuDialogStartIndex(0)
        SetMenuDialogDefaultIndex(0)
    ElseIf option == pluginOID && targetItem
        menuMode = 2
        SetMenuDialogOptions(JGJGTransmogNative.QueryPlugins(targetItem))
        SetMenuDialogStartIndex(0)
        SetMenuDialogDefaultIndex(0)
    ElseIf option == browseOID && targetItem
        menuMode = 3
        String[] names = JGJGTransmogNative.QueryNames(targetItem, "", filterPlugin, pageIndex)
        If names.Length == 0
            names = new String[1]
            names[0] = "No compatible results; build the catalogue first"
        EndIf
        SetMenuDialogOptions(names)
        SetMenuDialogStartIndex(0)
        SetMenuDialogDefaultIndex(0)
    ElseIf option == presetOID
        menuMode = 4
        SetMenuDialogOptions(JGJGTransmogNative.GetArmorPresetNames())
        SetMenuDialogStartIndex(selectedPreset)
        SetMenuDialogDefaultIndex(selectedPreset)
    EndIf
EndEvent

Event OnOptionMenuAccept(Int option, Int index)
    If option == categoryOID && menuMode == 0
        If index == 0
            weaponMode = 0
            targetLabel = "Select worn armor"
        Else
            weaponMode = 1
            targetLabel = "Select equipped weapon"
        EndIf
        targetItem = None
        selectedAppearance = None
        selectedName = "None"
        pageIndex = 0
        filterPlugin = ""
        filterLabel = "All mods"
    ElseIf option == targetOID && menuMode == 1
        If index < menuTargets.Length
            If weaponMode == 1 && targetItem
                JGJGTransmogNative.CancelWeaponPreview(targetItem)
            ElseIf weaponMode == 0 && targetItem
                JGJGTransmogNative.CancelArmorPreview(targetItem)
            EndIf
            targetItem = menuTargets[index]
            If index < menuTargetNames.Length
                targetLabel = menuTargetNames[index]
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
            String[] mods = JGJGTransmogNative.QueryPlugins(targetItem)
            If index < mods.Length
                filterPlugin = mods[index]
                filterLabel = filterPlugin
            EndIf
        EndIf
        pageIndex = 0
    ElseIf option == browseOID && menuMode == 3 && targetItem
        Form[] forms = JGJGTransmogNative.QueryForms(targetItem, "", filterPlugin, pageIndex)
        String[] names = JGJGTransmogNative.QueryNames(targetItem, "", filterPlugin, pageIndex)
        If index < forms.Length
            If weaponMode == 1
                JGJGTransmogNative.CancelWeaponPreview(targetItem)
            Else
                JGJGTransmogNative.CancelArmorPreview(targetItem)
            EndIf
            selectedAppearance = forms[index]
            If index < names.Length
                selectedName = names[index]
            EndIf
        EndIf
    ElseIf option == presetOID && menuMode == 4
        If index >= 0 && index < 10
            selectedPreset = index
            String[] names = JGJGTransmogNative.GetArmorPresetNames()
            If index < names.Length
                presetLabel = names[index]
            EndIf
        EndIf
    EndIf
    ForcePageReset()
EndEvent
