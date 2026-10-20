#include "ComparisonPreview.h"

#include "ArmorAppearance.h"
#include "WeaponAppearance.h"

#include <RE/J/JournalMenu.h>
#include <RE/U/UIMessageQueue.h>

#include <numbers>
#include <mutex>

namespace jgjg::engine {
namespace {
	std::mutex comparisonMutex;
	RE::NiPointer<RE::NiNode> sceneParent;
	RE::NiPointer<RE::NiAVObject> originalModel;
	RE::NiPointer<RE::NiAVObject> selectedModel;
	RE::NiPointer<RE::NiAVObject> hiddenPlayerModel;

	RE::NiPointer<RE::NiAVObject> ClonePlayerModel()
	{
		auto* player = RE::PlayerCharacter::GetSingleton();
		auto* model = player ? player->Get3D() : nullptr;
		return model ? RE::NiPointer<RE::NiAVObject>(model->Clone()) : nullptr;
	}

	void PlaceClone(RE::NiAVObject* clone, const RE::NiTransform& parentInverse,
	                const RE::NiTransform& playerWorld, const RE::NiPoint3& offset)
	{
		RE::NiTransform target = playerWorld;
		const auto translated = playerWorld.rotate * offset;
		target.translate.x += translated.x;
		target.translate.y += translated.y;
		target.translate.z += translated.z;

		RE::NiMatrix3 faceCamera;
		faceCamera.SetEulerAnglesXYZ(0.0f, 0.0f, std::numbers::pi_v<float>);
		target.rotate = target.rotate * faceCamera;
		clone->parent = nullptr;
		clone->parentIndex = 0;
		clone->local = parentInverse * target;
		clone->SetUserData(nullptr);
		clone->SetAppCulled(false);
	}

	struct MenuSink final : RE::BSTEventSink<RE::MenuOpenCloseEvent> {
		RE::BSEventNotifyControl ProcessEvent(const RE::MenuOpenCloseEvent* event,
		                                     RE::BSTEventSource<RE::MenuOpenCloseEvent>*) override
		{
			if (event && event->opening && event->menuName == RE::JournalMenu::MENU_NAME)
				EndSideBySideComparison();
			return RE::BSEventNotifyControl::kContinue;
		}
	};

	MenuSink menuSink;
}

bool InstallComparisonPreviewListener()
{
	auto* ui = RE::UI::GetSingleton();
	if (!ui) return false;
	ui->AddEventSink<RE::MenuOpenCloseEvent>(&menuSink);
	return true;
}

void EndSideBySideComparison()
{
	std::lock_guard lock(comparisonMutex);
	if (sceneParent) {
		if (originalModel) sceneParent->DetachChild(originalModel.get());
		if (selectedModel) sceneParent->DetachChild(selectedModel.get());
	}
	if (hiddenPlayerModel) hiddenPlayerModel->SetAppCulled(false);
	originalModel.reset();
	selectedModel.reset();
	hiddenPlayerModel.reset();
	sceneParent.reset();
}

bool StartSideBySideComparison(RE::TESForm* original, RE::TESForm* appearance)
{
	EndSideBySideComparison();
	if (!original || !appearance) return false;

	const bool armor = original->As<RE::TESObjectARMO>() != nullptr;
	const bool weapon = original->As<RE::TESObjectWEAP>() != nullptr;
	if (!armor && !weapon) return false;
	if ((armor && !appearance->As<RE::TESObjectARMO>()) ||
	    (weapon && !appearance->As<RE::TESObjectWEAP>())) return false;

	const auto preview = [armor, original](RE::TESForm* form) {
		return armor ? PreviewArmorAppearance(original, form) : PreviewWeaponAppearance(original, form);
	};
	const auto cancel = [armor, original] {
		if (armor) CancelArmorPreview(original);
		else CancelWeaponPreview(original);
	};

	if (!preview(original)) return false;
	auto before = ClonePlayerModel();
	if (!preview(appearance)) {
		cancel();
		return false;
	}
	auto after = ClonePlayerModel();
	cancel();
	if (!before || !after) return false;

	auto* player = RE::PlayerCharacter::GetSingleton();
	auto* playerRoot = player ? player->Get3D() : nullptr;
	auto* parent = playerRoot ? playerRoot->parent : nullptr;
	if (!playerRoot || !parent) return false;

	auto* queue = RE::UIMessageQueue::GetSingleton();
	if (!queue) return false;
	{
		std::lock_guard lock(comparisonMutex);
		sceneParent = RE::NiPointer<RE::NiNode>(parent);
		originalModel = std::move(before);
		selectedModel = std::move(after);
		hiddenPlayerModel = RE::NiPointer<RE::NiAVObject>(playerRoot);
		const auto parentInverse = parent->world.Invert();
		const auto playerWorld = playerRoot->world;
		PlaceClone(originalModel.get(), parentInverse, playerWorld, RE::NiPoint3(-72.0f, 0.0f, 0.0f));
		PlaceClone(selectedModel.get(), parentInverse, playerWorld, RE::NiPoint3(72.0f, 0.0f, 0.0f));
		parent->AttachChild(originalModel.get());
		parent->AttachChild(selectedModel.get());
		playerRoot->SetAppCulled(true);
	}
	queue->AddMessage(RE::JournalMenu::MENU_NAME, RE::UI_MESSAGE_TYPE::kHide, nullptr);
	return true;
}

}  // namespace jgjg::engine
