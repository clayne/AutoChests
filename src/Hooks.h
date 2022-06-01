#pragma once

using IngrEntryData = RE::CraftingSubMenus::AlchemyMenu::IngrEntryData;

class AutoChests
{
	using chests_t = std::vector<std::pair<RE::ObjectRefHandle, int>>;

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

	static bool is_player_owned([[maybe_unused]] RE::TESObjectREFR& refr)
	{
#ifdef DEBUG
		return true;
#else
		return refr.IsAnOwner(RE::PlayerCharacter::GetSingleton(), true, false);
#endif  // DEBUG
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
		uint32_t handle;
		refr->RemoveItem(&handle, item, cur_count, RE::ITEM_REMOVE_REASON::kRemove, nullptr, nullptr);
		return cur_count;
	}

	static bool add_item(RE::TESObjectREFR* refr, RE::TESBoundObject* item, uint32_t count)
	{
		if (!get_count(refr, item))
			return false;

		refr->AddObjectToContainer(item, nullptr, count, nullptr);
		return true;
	}

	static int get_container_size_origin(RE::TESObjectREFR* refr, char use_datahandler, char a3)
	{
		return _generic_foo_<19274, decltype(get_container_size_origin)>::eval(refr, use_datahandler, a3);
	}

	static RE::InventoryEntryData* get_container_item_origin(RE::TESObjectREFR* refr, uint32_t ind, char use_datahandler)
	{
		return _generic_foo_<19273, decltype(get_container_item_origin)>::eval(refr, ind, use_datahandler);
	}

public:
	static void ForEachAutoChest(std::function<bool(RE::TESObjectREFR* refr)> callback)
	{
		for (auto& i : autochests) {
			auto refr = i.first.get().get();
			if (refr && !callback(refr)) {
				return;
			}
		}
	}

	static void init_chests()
	{
		autochests.clear();

		RE::TES::GetSingleton()->ForEachReference([=](RE::TESObjectREFR& refr) {
			if (!refr.IsDisabled() && is_chest(refr) && is_near(refr) && is_player_owned(refr)) {
				auto size = get_container_size_origin(&refr, 0, 1);
				autochests.push_back({ refr.GetHandle(), size });
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

	static int get_container_size()
	{
		int ans = 0;
		for (auto& i : autochests) {
			ans += i.second;
		}
		return ans;
	}

	static RE::InventoryEntryData* get_container_item(int ind)
	{
		for (auto& i : autochests) {
			if (ind >= i.second) {
				ind -= i.second;
			} else {
				auto refr = i.first.get().get();
				auto ans = get_container_item_origin(refr, ind, 0);
				return ans;
			}
		}

		return nullptr;
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
		t.write_call<6>(REL::ID(16562).address() + 0x91, AddItem);                                                          // SkyrimSE.exe+20AE81
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
		AutoChests::init_chests();
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

class AlchemyHook
{
	static bool is_same(RE::CraftingSubMenus::AlchemyMenu* menu, uint32_t i, uint32_t ind)
	{
		return menu->ingrs[i].item->object->formID == menu->ingrs[ind].item->object->formID;
	}

	static bool wrong_choise(RE::CraftingSubMenus::AlchemyMenu* menu)
	{
		auto& chosen = menu->chosenInds;
		switch (chosen.size()) {
		case 0:
		case 1:
			return false;
		case 2:
			return is_same(menu, chosen[0], chosen[1]);
		case 3:
			return is_same(menu, chosen[0], chosen[1]) || is_same(menu, chosen[1], chosen[2]) || is_same(menu, chosen[0], chosen[2]);
		default:
			return false;
		}
	}

public:
	static void Hook()
	{
		auto& t = SKSE::GetTrampoline();

		t.write_call<6>(REL::ID(50449).address() + 0x3ef, RemoveItem);                               // SkyrimSE.exe+86BD6F
		_on_item_clicked = t.write_call<5>(REL::ID(50439).address() + 0x2a, on_item_clicked);        // SkyrimSE.exe+86B1CA
		_AddItem = t.write_call<5>(REL::ID(50449).address() + 0x1cf, AddItem);                       // SkyrimSE.exe+86BB4F
		_get_container_size = t.write_call<5>(REL::ID(50453).address() + 0xaf, get_container_size);  // SkyrimSE.exe+86CE6F
		_get_container_item = t.write_call<5>(REL::ID(50453).address() + 0xdd, get_container_item);  // SkyrimSE.exe+86CE9D

		// prevent from creating potions
		{
			// SkyrimSE.exe+86B9C4
			uintptr_t retaddr_noskip = REL::ID(50449).address() + 0x44;

			// SkyrimSE.exe+86C623
			uintptr_t retaddr_skip = REL::ID(50449).address() + 0xca3;

			struct Code : Xbyak::CodeGenerator
			{
				Code(uintptr_t func_addr, uintptr_t retaddr_skip, uintptr_t retaddr_noskip)
				{
					Xbyak::Label L__Skip;

					cmp(ptr[rcx + 0x170], rsi);
					jz(L__Skip);

					mov(rax, func_addr);
					call(rax);
					test(al, al);
					jnz(L__Skip);
					mov(rax, retaddr_noskip);
					jmp(rax);

					L(L__Skip);
					mov(rax, retaddr_skip);
					jmp(rax);
				}
			} xbyakCode{ uintptr_t(wrong_choise), retaddr_skip, retaddr_noskip };

			add_trampoline<5, 50449, 0x37>(&xbyakCode);  // SkyrimSE.exe+86B9B7
		}
	}

private:

	static void AddItem(RE::Actor* a, RE::TESBoundObject* item, RE::ExtraDataList* extraList, int count, RE::TESObjectREFR* fromRefr)
	{
		if (!AutoChests::add_item(item, count))
			return _AddItem(a, item, extraList, count, fromRefr);
	}

	static uint32_t* RemoveItem(RE::Character* a, uint32_t* ref_handle, RE::TESBoundObject* item, int count, RE::ITEM_REMOVE_REASON reason, RE::ExtraDataList* extraList, RE::TESObjectREFR* refr, RE::NiPoint3* pos, RE::NiPoint3* rot)
	{
		count = AutoChests::remove_item(item, count);
		if (count > 0)
			a->RemoveItem(ref_handle, item, count, reason, extraList, refr, pos, rot);
		return ref_handle;
	}

	static void on_item_clicked(RE::CraftingSubMenus::AlchemyMenu* menu, uint32_t ind) {
		if (ind >= menu->ingrs.size())
			return _on_item_clicked(menu, ind);

		auto& chosen = menu->chosenInds;
		uint32_t i = 0;
		for (i; i < chosen.size(); i++) {
			if (chosen[i] == ind)
				break;
		}

		if (i < chosen.size() || chosen.size() >= 3)
			return _on_item_clicked(menu, ind);

		for (i = 0; i < chosen.size(); i++) {
			if (is_same(menu, chosen[i], ind))
				break;
		}

		if (i >= chosen.size())
			return _on_item_clicked(menu, ind);
	}

	static int get_container_size(RE::TESObjectREFR* player, char use_datahandler, char a3)
	{
		AutoChests::init_chests();

		return _get_container_size(player, use_datahandler, a3) + AutoChests::get_container_size();
	}

	static RE::InventoryEntryData* get_container_item(RE::TESObjectREFR* player, int ind, char use_datahandler)
	{
		int chests_size = AutoChests::get_container_size();

		if (ind >= chests_size) {
			auto ans = _get_container_item(player, ind - chests_size, use_datahandler);
			return ans;
		} else {
			return AutoChests::get_container_item(ind);
		}
	}

	static inline REL::Relocation<decltype(get_container_size)> _get_container_size;
	static inline REL::Relocation<decltype(get_container_item)> _get_container_item;
	static inline REL::Relocation<decltype(on_item_clicked)> _on_item_clicked;
	static inline REL::Relocation<decltype(AddItem)> _AddItem;
};

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
		AutoChests::init_chests();

		AutoChests::ForEachAutoChest([=](RE::TESObjectREFR* refr) -> bool {
			draw_line(refr->GetPosition(), a->GetPosition(), 5.0f, 0);
			return true;
			});

		_Update(a, delta);
		DebugAPI_IMPL::DebugAPI::Update();
	}

	static inline REL::Relocation<decltype(Update)> _Update;
};

