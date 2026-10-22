Scriptname JGJGTransmogNative Hidden
Bool Function RequestCatalogue() Global Native
Int Function CatalogueCount() Global Native
Bool Function IsBuilding() Global Native
String Function Status() Global Native
String Function GetArmorRenderDiagnostics() Global Native
String Function GetWeaponRenderDiagnostics() Global Native
String Function GetArmorRenderFormIDs() Global Native
String Function GetArmorRenderContext() Global Native
String Function GetArmorSourceAddonMasks() Global Native
String Function GetArmorAppearanceAddonMasks() Global Native
String Function GetArmorHookAddonMasks() Global Native
String Function GetPlayerWornArmorFormIDs() Global Native
String Function GetWeaponRenderFormIDs() Global Native
String Function GetArmorOverrideStatus() Global Native
String Function GetWeaponSetFormIDs() Global Native
String Function GetWeaponHookFormIDs() Global Native
String Function GetWeaponOverrideStatus() Global Native
String[] Function QueryNames(Form target, String search, String plugin, Int page) Global Native
String[] Function QueryPlugins(Form target) Global Native
Int Function QueryPageCount(Form target, String plugin) Global Native
Form[] Function QueryForms(Form target, String search, String plugin, Int page) Global Native
Form[] Function GetPlayerWornArmors() Global Native
String[] Function GetPlayerWornArmorNames() Global Native
Form[] Function GetPlayerWornWeapons() Global Native
String[] Function GetPlayerWornWeaponNames() Global Native
Bool Function SetArmorAppearance(Form original, Form appearance) Global Native
Bool Function ClearArmorAppearance(Form original) Global Native
Bool Function PreviewArmorAppearance(Form original, Form appearance) Global Native
Bool Function CancelArmorPreview(Form original) Global Native
String[] Function GetArmorPresetNames() Global Native
Bool Function SaveArmorPreset(Int slot) Global Native
Bool Function LoadArmorPreset(Int slot) Global Native
Int Function GetLastPresetSkipped() Global Native
Bool Function SetWeaponAppearance(Form original, Form appearance) Global Native
Bool Function ClearWeaponAppearance(Form original) Global Native
Bool Function PreviewWeaponAppearance(Form original, Form appearance) Global Native
Bool Function CancelWeaponPreview(Form original) Global Native
