#pragma once

class AutoChests
{
	using chests_t = std::vector<RE::ObjectRefHandle>;

	static inline chests_t autochests = chests_t();

	static bool is_chest(RE::TESObjectREFR& refr)
	{
		return refr.GetBaseObject() && (int)refr.GetBaseObject()->formType.underlying() == 28;
	}

	static bool is_near(RE::TESObjectREFR& refr)
	{
		auto player = RE::PlayerCharacter::GetSingleton();
		return FenixUtils::get_dist2(player, &refr) < 2000000.0;
	}

	static bool is_player_owned(RE::TESObjectREFR& refr)
	{
		return refr.IsAnOwner(RE::PlayerCharacter::GetSingleton(), true, false);
	}

	static uint32_t get_count_(RE::InventoryChanges* changes, RE::TESBoundObject* item)
	{
		return _generic_foo_<15868, decltype(get_count_)>::eval(changes, item);
	}

	static uint32_t get_count(RE::TESObjectREFR* refr, RE::TESBoundObject* item)
	{
		auto changes = refr->GetInventoryChanges();
		return get_count_(changes, item);
	}

	static uint32_t remove_item(RE::TESObjectREFR* refr, RE::TESBoundObject* item, uint32_t count)
	{
		auto cur_count = std::min(get_count(refr, item), count);
		refr->RemoveItem(item, cur_count, RE::ITEM_REMOVE_REASON::kRemove, nullptr, nullptr );
		return cur_count;
	}

	static bool add_item(RE::TESObjectREFR* refr, RE::TESBoundObject* item, uint32_t count)
	{
		if (!get_count(refr, item))
			return false;

		refr->AddObjectToContainer(item, nullptr, count, nullptr);
		return true;
	}

	static void ForEachAutoChest(std::function<bool(RE::TESObjectREFR* refr)> callback)
	{
		for (auto handle : autochests) {
			auto refr = handle.get().get();
			if (refr && !callback(refr)) {
				return;
			}
		}
	}

public:
	static void update_chests()
	{
		autochests.clear();

		RE::TES::GetSingleton()->ForEachReference([=](RE::TESObjectREFR& refr) {
			if (!refr.IsDisabled() && is_chest(refr) && is_near(refr) && is_player_owned(refr)) {
				autochests.push_back(refr.GetHandle());
			}
			return true;
		});
	}

	static uint32_t get_count(RE::TESBoundObject* item) {
		uint32_t ans = 0;
		
		ForEachAutoChest([=, &ans](RE::TESObjectREFR * refr) {
			ans += get_count(refr, item);
			return true;
			});

		return ans;
	}

	static uint32_t remove_item(RE::TESBoundObject* item, uint32_t count)
	{
		ForEachAutoChest([=, &count](RE::TESObjectREFR* refr) {
			count -= remove_item(refr, item, count);
			return count > 0;
		});
		return count;
	}

	static bool add_item(RE::TESBoundObject* item, uint32_t count)
	{
		bool added = false;
		ForEachAutoChest([=, &added](RE::TESObjectREFR* refr) {
			if (add_item(refr, item, count)) {
				added = true;
				return false;
			}
			return true;
		});

		return added;
	}
};

class AutoChestsHook
{
public:
	static void Hook()
	{
		auto& t = SKSE::GetTrampoline();

		_get_count_add_craft_descr = t.write_call<5>(REL::ID(16565).address() + 0x64, get_count_add_craft_descr);           // SkyrimSE.exe+20B004
		_get_count_has_enough_items = t.write_call<5>(REL::ID(50372).address() + 0x41, get_count_has_enough_items);         // SkyrimSE.exe+868E61
		_get_CurrentFurnitureRefHandle = t.write_call<5>(REL::ID(50274).address() + 0x148, get_CurrentFurnitureRefHandle);  // SkyrimSE.exe+862DC8
		_RemoveItem = t.write_call<5>(REL::ID(16562).address() + 0x5e, RemoveItem);                                         // SkyrimSE.exe+20AE4E
		t.write_call<6>(REL::ID(16562).address() + 0x91, AddItem);                                               // SkyrimSE.exe+20AE81
	}

private:
	static uint32_t get_count_add_craft_descr(RE::InventoryChanges* changes, RE::TESBoundObject* item, void* filter)
	{
		return _get_count_add_craft_descr(changes, item, filter) + AutoChests::get_count(item);
	}

	static uint32_t get_count_has_enough_items(RE::InventoryChanges* changes, RE::TESBoundObject* item, void* filter)
	{
		return _get_count_has_enough_items(changes, item, filter) + AutoChests::get_count(item);
	}

	static uint32_t* get_CurrentFurnitureRefHandle(RE::AIProcess* a1, uint32_t* ans)
	{
		AutoChests::update_chests();
		return _get_CurrentFurnitureRefHandle(a1, ans);
	}

	static uint32_t RemoveItem(RE::TESBoundObject* item, uint32_t count) {
		count = AutoChests::remove_item(item, count);
		if (count > 0)
			_RemoveItem(item, count);
		return 1;
	}

	static void AddItem(RE::Actor* a, RE::TESBoundObject* item, RE::ExtraDataList* extraList, int count, RE::TESObjectREFR* fromRefr)
	{
		if (!AutoChests::add_item(item, count))
			_generic_foo_<36525, decltype(AddItem)>::eval(a, item, extraList, count, fromRefr);
	}

	static inline REL::Relocation<decltype(get_count_add_craft_descr)> _get_count_add_craft_descr;
	static inline REL::Relocation<decltype(get_count_has_enough_items)> _get_count_has_enough_items;
	static inline REL::Relocation<decltype(get_CurrentFurnitureRefHandle)> _get_CurrentFurnitureRefHandle;
	static inline REL::Relocation<decltype(RemoveItem)> _RemoveItem;
};

/*
class PlayerCharacterHook
{
public:
	static void Hook()
	{
		_Update = REL::Relocation<uintptr_t>(REL::ID(RE::VTABLE_PlayerCharacter[0])).write_vfunc(0xad, Update);
	}

private:
	static void Update(RE::PlayerCharacter* a, float delta)
	{
		AutoChests::update_chests();

		auto& autochests = AutoChests::get_chests();
		for (auto handle : autochests) {
			auto refr = handle.get().get();
			if (refr) {
				draw_line(refr->GetPosition(), a->GetPosition(), 5.0f, 0);
			} else {
				draw_line(a->GetPosition(), a->GetPosition() + RE::NiPoint3{ 0.0f, 0.0f, 10.0f }, 5.0f, 0);
			}
		}

		_Update(a, delta);
		DebugAPI_IMPL::DebugAPI::Update();
	}

	static inline REL::Relocation<decltype(Update)> _Update;
};
*/
