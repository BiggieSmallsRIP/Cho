#include "../SDK/PluginSDK.h"
#include "../SDK/EventArgs.h"
#include "../SDK/EventHandler.h"

PLUGIN_API const char PLUGIN_PRINT_NAME[32] = "Cho'Gath";
PLUGIN_API const char PLUGIN_PRINT_AUTHOR[32] = "Biggiesmalls";
PLUGIN_API ChampionId PLUGIN_TARGET_CHAMP = ChampionId::Chogath;

namespace Menu
{
	IMenu* MenuInstance = nullptr;

	namespace Combo
	{
		IMenuElement* Enabled = nullptr;
		IMenuElement* UseQ = nullptr;
		IMenuElement* UseW = nullptr;
		IMenuElement* UseE = nullptr;
		IMenuElement* UseR = nullptr;
	}

	namespace Harass
	{
		IMenuElement* Enabled = nullptr;
		IMenuElement* UseQ = nullptr;
		IMenuElement* UseW = nullptr;
		IMenuElement* UseE = nullptr;
		IMenuElement* TowerHarass = nullptr;
		IMenuElement* ManaQ = nullptr;
	}

	namespace LaneClear
	{
		IMenuElement* Enabled = nullptr;
		IMenuElement* UseQ = nullptr;
		IMenuElement* UseW = nullptr;
		IMenuElement* UseE = nullptr;
		IMenuElement* ManaClear = nullptr;
		IMenuElement* MinMinions = nullptr;
	}

	namespace Killsteal
	{
		IMenuElement* Enabled = nullptr;
		IMenuElement* UseQ = nullptr;
		IMenuElement* UseW = nullptr;
		IMenuElement* UseR = nullptr;
	}

	namespace Misc
	{
		IMenuElement* Winterrupt = nullptr;
		IMenuElement* Antigap = nullptr;
		IMenuElement* AutoHardCC = nullptr;
		IMenuElement* JungleKS = nullptr;
		IMenuElement* Items = nullptr;
	}
	namespace Drawings
	{
		IMenuElement* Toggle = nullptr;
		IMenuElement* DrawQRange = nullptr;
		IMenuElement* DrawWRange = nullptr;
		IMenuElement* DrawERange = nullptr;
		IMenuElement* DrawRRange = nullptr;
	}

	namespace Colors
	{
		IMenuElement* QColor = nullptr;
		IMenuElement* WColor = nullptr;
		IMenuElement* EColor = nullptr;
		IMenuElement* RColor = nullptr;
	}
}

namespace Spells
{
	std::shared_ptr<ISpell> Q = nullptr;
	std::shared_ptr<ISpell> W = nullptr;
	std::shared_ptr<ISpell> E = nullptr;
	std::shared_ptr<ISpell> R = nullptr;
}
int ult_stacks()
{
	auto ult_stacks = 0;
	if (g_LocalPlayer->HasBuff(hash("Feast")))
		return ult_stacks = g_LocalPlayer->GetBuff(hash("Feast")).Count;
}
//number of  enemies in Range
int CountEnemiesInRange(const Vector& Position, const float Range)
{
	auto Enemies = g_ObjectManager->GetChampions(false);
	int Counter = 0;
	for (auto& Enemy : Enemies)
		if (Enemy->IsVisible() && Enemy->IsValidTarget() && Position.Distance(Enemy->Position()) <= Range)
			Counter++;
	return Counter;
}

//Extending Q position if slowed
Vector PredictedPostion(IGameObject* unit)
{
	return unit->ServerPosition().Extend(g_LocalPlayer->ServerPosition(), -30.f);
}

// returns prediction of spell
auto get_prediction(std::shared_ptr<ISpell> spell, IGameObject* unit) -> IPredictionOutput {
	return g_Common->GetPrediction(unit, spell->Range(), spell->Delay(), spell->Radius(), spell->Speed(), spell->CollisionFlags(),
		g_LocalPlayer->ServerPosition());
}

// killsteal
void KillstealLogic()
{
	const auto Enemies = g_ObjectManager->GetChampions(false);
	for (auto Enemy : Enemies)
	{
		if (Menu::Killsteal::UseR->GetBool() && Spells::R->IsReady() && Enemy->IsValidTarget(Spells::R->Range()) && !Enemy->IsInvulnerable() && !Enemy->HasBuff("KayleR") && !Enemy->HasBuff("UndyingRage"))
		{
			auto player_data = g_LocalPlayer->GetCharacterData();
			auto base_health = g_LocalPlayer->GetCharacterData().BaseHealth;
			auto bonus_health = g_LocalPlayer->MaxHealth() - base_health;

			auto damage = g_Common->CalculateDamageOnUnit(g_LocalPlayer, Enemy, DamageType::True, std::vector<double> {290, 460, 630}[Spells::R->Level()
				- 1] + (0.5 * g_LocalPlayer->FlatMagicDamageMod()) + (0.08 * bonus_health));

			if (damage >= Enemy->Health())
				Spells::R->Cast(Enemy);
		}

		if (Menu::Killsteal::UseQ->GetBool() && Spells::Q->IsReady() && Enemy->IsValidTarget(Spells::Q->Range()))
		{
			auto QDamage = g_Common->GetSpellDamage(g_LocalPlayer, Enemy, SpellSlot::Q, false);
			if (QDamage >= Enemy->RealHealth(false, true))
				Spells::Q->Cast(Enemy, HitChance::VeryHigh);
		}
		if (Menu::Killsteal::UseW->GetBool() && Spells::W->IsReady() && Enemy->IsValidTarget(Spells::W->Range()))
		{
			auto WDamage = g_Common->GetSpellDamage(g_LocalPlayer, Enemy, SpellSlot::W, false);
			if (WDamage >= Enemy->RealHealth(false, true))
				Spells::W->Cast(Enemy, HitChance::VeryHigh);
		}
	}
}

void MiscLogic()
{
	const auto Enemies = g_ObjectManager->GetChampions(false);
	for (auto Enemy : Enemies)
	{
		if (Spells::Q->IsReady() && Enemy && Enemy->IsAIHero() && (g_Common->TickCount() - Spells::Q->LastCastTime() >= 500))
		{
			if (Enemy && Enemy->IsValidTarget(Spells::Q->Range()))
			{
				const auto pred = get_prediction(Spells::Q, Enemy);

				if (Menu::Misc::Antigap->GetBool())
					if (pred.Hitchance == HitChance::Dashing)
						Spells::Q->Cast(pred.CastPosition);

				if (Menu::Misc::AutoHardCC->GetBool())
					if (pred.Hitchance == HitChance::Immobile)
						Spells::Q->Cast(pred.CastPosition);

				if (Enemy->HasBuff("vorpalspikesdebuff"))
					Spells::Q->Cast(Enemy, HitChance::VeryHigh);
			}
		}

		if (Spells::W->IsReady())
		{
			if (Enemy  && Enemy->IsAIHero() && Enemy->IsValidTarget(Spells::W->Range()))
			{
				if (Menu::Misc::Winterrupt->GetBool())
					if (Enemy->IsCastingInterruptibleSpell())
						Spells::W->Cast(Enemy->ServerPosition());
			}
		}
	}

	// Eat baron/dragon

	if (Menu::Misc::JungleKS->GetBool())
	{
		const auto Enemies = g_ObjectManager->GetJungleMobs();
		for (auto Enemy : Enemies)
		{
			if (Menu::Misc::JungleKS->GetBool() && Spells::R->IsReady() && Enemy->IsInRange(Spells::R->Range()) && Enemy->IsEpicMonster())
			{
				auto player_data = g_LocalPlayer->GetCharacterData();
				auto base_health = g_LocalPlayer->GetCharacterData().HealthPerLevel * g_LocalPlayer->Level();
				auto bonus_health = g_LocalPlayer->MaxHealth() - base_health;

				auto damage = g_Common->CalculateDamageOnUnit(g_LocalPlayer, Enemy, DamageType::True, std::vector<double> {1000, 1000, 1000}[Spells::R->Level()
					- 1] + (0.5 * g_LocalPlayer->FlatMagicDamageMod()) + (0.09 * bonus_health));

				if (Enemy->IsValidTarget() && damage >= Enemy->Health())
					Spells::R->Cast(Enemy);
			}
		}
	}
}

// combo
void ComboLogic()
{
	if (Menu::Combo::Enabled->GetBool())
	{
		if (Menu::Combo::UseR->GetBool() && Spells::R->IsReady())
		{
			auto Target = g_Common->GetTarget(Spells::R->Range(), DamageType::True);
			if (Target && Target->IsValidTarget() && Target->IsAIHero())
			{
				if (CountEnemiesInRange(g_LocalPlayer->Position(), 330.f) >= 2 && g_LocalPlayer->HealthPercent() <= 15)
					Spells::R->Cast(Target);
			}
		}

		if (Menu::Combo::UseQ->GetBool() && Spells::Q->IsReady())
		{
			auto Target = g_Common->GetTargetFromCoreTS(Spells::Q->Range(), DamageType::Magical);
			if (Target && Target->IsValidTarget() && Target->IsAIHero() && Target->Distance(g_LocalPlayer) <= 875.f)
				if (g_Common->TickCount() - Spells::Q->LastCastTime() >= 500)
				{
					Spells::Q->Cast(Target, HitChance::VeryHigh);
				}
		}

		if (Menu::Combo::UseW->GetBool() && Spells::W->IsReady())
		{
			auto Target = g_Common->GetTarget(Spells::W->Range(), DamageType::Magical);
			if (Target && Target->IsValidTarget() && Target->IsAIHero())
				if (g_Common->TickCount() - Spells::W->LastCastTime() >= 500)
			{
				if (Target->Distance(g_LocalPlayer) <= 570.f)
					Spells::W->Cast(Target, HitChance::High);
			}
		}
	}

	//items
	{
		auto Enemies = g_ObjectManager->GetChampions(false);
		for (auto& Enemy : Enemies)
		{
			if (Menu::Misc::Items->GetBool() && Enemy && Enemy->IsValidTarget() && g_LocalPlayer->HasItem(ItemId::Hextech_GLP_800))
			{
				if (Enemy->Distance(g_LocalPlayer->Position()) < 650.f)
					g_LocalPlayer->CastItem(ItemId::Hextech_GLP_800, Enemy);
			}
		}
	}
}

// harass
void HarassLogic()
{
	if (Menu::Harass::TowerHarass->GetBool() && g_LocalPlayer->IsUnderMyEnemyTurret())
		return;

	if (g_LocalPlayer->ManaPercent() < Menu::Harass::ManaQ->GetInt())
		return;

	if (Menu::Harass::Enabled->GetBool())
	{
		if (Menu::Harass::UseW->GetBool() && Spells::W->IsReady())
		{
			auto Target = g_Common->GetTarget(Spells::W->Range(), DamageType::Magical);
			if (Target && Target->IsValidTarget())
				if (g_Common->TickCount() - Spells::W->LastCastTime() >= 500)
			{
				if (Target->Distance(g_LocalPlayer) <= 550.f)
					Spells::W->Cast(Target, HitChance::High);
			}
		}

		if (Menu::Harass::UseQ->GetBool() && Spells::Q->IsReady())
		{
			auto Target = g_Common->GetTarget(Spells::Q->Range(), DamageType::Magical);
			if (Target && Target->IsValidTarget() && Target->IsAIHero() && Target->HasBuffOfType(BuffType::Slow))
				if (g_Common->TickCount() - Spells::Q->LastCastTime() >= 500)
			{	
				if (Target->Distance(g_LocalPlayer) <= 875.f)
					Spells::Q->Cast(Target, HitChance::VeryHigh);
			}
		}
	}
}

// E after AA
void OnAfterAttack(IGameObject* target)
{
	if (!g_LocalPlayer->HasBuff("VorpalSpikes"))
	{
		if (Menu::Combo::Enabled->GetBool() && Spells::E->IsReady())
		{
			if (g_Orbwalker->IsModeActive(eOrbwalkingMode::kModeCombo) || g_Orbwalker->IsModeActive(eOrbwalkingMode::kModeHarass))
			{
				const auto OrbwalkerTarget = g_Orbwalker->GetTarget();
				if (OrbwalkerTarget && OrbwalkerTarget->IsAIHero())
				{
					if (Menu::Combo::UseE->GetBool())
					{
						Spells::E->Cast();
						g_Orbwalker->ResetAA();
					}
				}
			}
		}
	}
}

// Lane Clear Logic
void LaneCLearLogic()
{
	if (g_LocalPlayer->ManaPercent() < Menu::LaneClear::ManaClear->GetInt())
		return;

	auto MinMinions = Menu::LaneClear::MinMinions->GetInt();
	if (!MinMinions)
		return;

	auto Target = g_Orbwalker->GetTarget();
	{
		{
			if (Target && Target->IsMinion() && Spells::Q->Radius() && Spells::Q->IsReady() && g_Orbwalker->IsModeActive(eOrbwalkingMode::kModeLaneClear))

				if (Menu::LaneClear::UseQ->GetBool())
					Spells::Q->CastOnBestFarmPosition(MinMinions);
		}
		{
			if (Target && Target->IsMinion() && Spells::W->Range() && Spells::W->IsReady() && g_Orbwalker->IsModeActive(eOrbwalkingMode::kModeLaneClear))

				if (Menu::LaneClear::UseW->GetBool())
					Spells::W->CastOnBestFarmPosition(MinMinions);
		}
	}

	// Jungle Clear Logic
	{
		auto Monster = g_Orbwalker->GetTarget();
		{
			if (Monster && Monster->IsMonster() && Menu::LaneClear::UseQ->GetBool() && g_Orbwalker->IsModeActive(eOrbwalkingMode::kModeLaneClear) && Spells::Q->IsReady())
				Spells::Q->Cast(Monster, HitChance::High);
		}
		{
			if (Monster && Monster->IsMonster() && Spells::W->IsReady() && Menu::LaneClear::UseW->GetBool() && Spells::W->Range() && g_Orbwalker->IsModeActive(eOrbwalkingMode::kModeLaneClear))

				Spells::W->Cast(Monster, HitChance::High);
		}
	}
}

void OnGameUpdate()
{
	if (g_LocalPlayer->IsDead())
		return;

	if (Spells::R->Level() == 1 && ult_stacks() <= 17)
		Spells::R->SetRange(250.f + 4.4f * ult_stacks());

	if (Spells::R->Level() == 2 && ult_stacks() <= 13)
		Spells::R->SetRange(250.f + 5.76f * ult_stacks());

	if (Spells::R->Level() == 3 && ult_stacks() <= 10)
		Spells::R->SetRange(250.f + 7.5f * ult_stacks());

	if (Menu::Killsteal::Enabled->GetBool())
		KillstealLogic();

	if (Menu::Combo::Enabled->GetBool() && g_Orbwalker->IsModeActive(eOrbwalkingMode::kModeCombo))
		ComboLogic();

	if (Menu::Harass::Enabled->GetBool() && g_Orbwalker->IsModeActive(eOrbwalkingMode::kModeHarass))
		HarassLogic();

	if (Menu::LaneClear::Enabled->GetBool() && g_Orbwalker->IsModeActive(eOrbwalkingMode::kModeLaneClear))
		LaneCLearLogic();

	MiscLogic();
}

// drawings
void OnHudDraw()
{
	if (!Menu::Drawings::Toggle->GetBool() || g_LocalPlayer->IsDead())
		return;

	const auto PlayerPosition = g_LocalPlayer->Position();
	const auto CirclesWidth = 1.5f;

	if (Menu::Drawings::DrawQRange->GetBool() && !Spells::Q->CooldownTime())
		g_Drawing->AddCircle(PlayerPosition, Spells::Q->Range(), Menu::Colors::QColor->GetColor(), CirclesWidth);

	if (Menu::Drawings::DrawWRange->GetBool() && !Spells::W->CooldownTime())
		g_Drawing->AddCircle(PlayerPosition, Spells::W->Range(), Menu::Colors::WColor->GetColor(), CirclesWidth);

	if (Menu::Drawings::DrawERange->GetBool() && !Spells::E->CooldownTime())
		g_Drawing->AddCircle(PlayerPosition, Spells::E->Range(), Menu::Colors::EColor->GetColor(), CirclesWidth);

	if (Menu::Drawings::DrawRRange->GetBool() && Spells::R->Level())
		g_Drawing->AddCircle(PlayerPosition, Spells::R->Range(), Menu::Colors::RColor->GetColor(), CirclesWidth);
}

PLUGIN_API bool OnLoadSDK(IPluginsSDK* plugin_sdk)
{
	DECLARE_GLOBALS(plugin_sdk);

	if (g_LocalPlayer->ChampionId() != ChampionId::Chogath)
		return false;

	using namespace Menu;
	using namespace Spells;

	MenuInstance = g_Menu->CreateMenu("Cho'Gath", "Cho'Gath by BiggieSmalls");

	const auto ComboSubMenu = MenuInstance->AddSubMenu("Combo", "combo_menu");
	Menu::Combo::Enabled = ComboSubMenu->AddCheckBox("Enable Combo", "enable_combo", true);
	Menu::Combo::UseQ = ComboSubMenu->AddCheckBox("Use Q", "combo_use_q", false);
	Menu::Combo::UseW = ComboSubMenu->AddCheckBox("Use W", "combo_use_w", true);
	Menu::Combo::UseE = ComboSubMenu->AddCheckBox("Use E to reset AA", "combo_use_e", true);
	Menu::Combo::UseR = ComboSubMenu->AddCheckBox("Use R before we die in teamfight", "combo_use_R", false);

	const auto HarassSubMenu = MenuInstance->AddSubMenu("Harass", "harass_menu");
	Menu::Harass::Enabled = HarassSubMenu->AddCheckBox("Enable Harass", "enable_harass", true);
	Menu::Harass::UseQ = HarassSubMenu->AddCheckBox("Use Q", "harass_use_q", false);
	Menu::Harass::UseW = HarassSubMenu->AddCheckBox("Use W", "harass_use_w", true);
	Menu::Harass::UseE = HarassSubMenu->AddCheckBox("Use E to reset AA", "harass_use_ea", true);
	Menu::Harass::TowerHarass = HarassSubMenu->AddCheckBox("Don't Harass under tower", "use_tower_q", true);
	Menu::Harass::ManaQ = HarassSubMenu->AddSlider("Min Mana", "min_mana_harass", 50, 0, 100, true);

	const auto LaneClearSubMenu = MenuInstance->AddSubMenu("Lane Clear", "laneclear_menu");
	Menu::LaneClear::Enabled = LaneClearSubMenu->AddCheckBox("Enable Lane Clear", "enable_laneclear", true);
	Menu::LaneClear::UseQ = LaneClearSubMenu->AddCheckBox("Use Q", "laneclear_use_q", false);
	Menu::LaneClear::UseW = LaneClearSubMenu->AddCheckBox("Use W", "laneclear_use_w", false);
	Menu::LaneClear::ManaClear = LaneClearSubMenu->AddSlider("Min Mana", "min_mana_laneclear", 50, 0, 100, true);
	Menu::LaneClear::MinMinions = LaneClearSubMenu->AddSlider("Min minions", "lane_clear_min_minions", 4, 0, 9);

	const auto KSSubMenu = MenuInstance->AddSubMenu("KS", "ks_menu");
	Menu::Killsteal::Enabled = KSSubMenu->AddCheckBox("Enable Killsteal", "enable_ks", true);
	Menu::Killsteal::UseQ = KSSubMenu->AddCheckBox("Use Q", "q_ks", true);
	Menu::Killsteal::UseW = KSSubMenu->AddCheckBox("Use W", "w_ks", true);
	Menu::Killsteal::UseR = KSSubMenu->AddCheckBox("Use R", "r_ks", true);

	const auto MiscSubMenu = MenuInstance->AddSubMenu("Misc", "misc_menu");
	Menu::Misc::Winterrupt = MiscSubMenu->AddCheckBox("Interrupt channels", "w_interupt", true);
	Menu::Misc::Antigap = MiscSubMenu->AddCheckBox("Antigapcloser", "q_dashing", true);
	Menu::Misc::AutoHardCC = MiscSubMenu->AddCheckBox("Auto Q on hard CC", "q_hard_cc", true);
	Menu::Misc::JungleKS = MiscSubMenu->AddCheckBox("Eat Epic Monsters", "r_epic_mob", true);
	Menu::Misc::Items = MiscSubMenu->AddCheckBox("Use GLP-800", "items_glp", true);

	const auto DrawingsSubMenu = MenuInstance->AddSubMenu("Drawings", "drawings_menu");
	Drawings::Toggle = DrawingsSubMenu->AddCheckBox("Enable Drawings", "drawings_toggle", true);
	Drawings::DrawQRange = DrawingsSubMenu->AddCheckBox("Draw Q Range", "draw_q", true);
	Drawings::DrawWRange = DrawingsSubMenu->AddCheckBox("Draw W Range", "draw_w", true);
	Drawings::DrawERange = DrawingsSubMenu->AddCheckBox("Draw E Range", "draw_e", true);
	Drawings::DrawRRange = DrawingsSubMenu->AddCheckBox("Draw R Range", "draw_r", true);

	const auto ColorsSubMenu = DrawingsSubMenu->AddSubMenu("Colors", "color_menu");
	Colors::QColor = ColorsSubMenu->AddColorPicker("Q Range", "color_q_range", 0, 175, 255, 180);
	Colors::WColor = ColorsSubMenu->AddColorPicker("W Range", "color_w_range", 0, 215, 155, 180);
	Colors::EColor = ColorsSubMenu->AddColorPicker("E Range", "color_e_range", 0, 75, 55, 180);
	Colors::RColor = ColorsSubMenu->AddColorPicker("R Range", "color_r_range", 200, 200, 200, 180);

	Spells::Q = g_Common->AddSpell(SpellSlot::Q, 950.f);
	Spells::W = g_Common->AddSpell(SpellSlot::W, 575.f);
	Spells::E = g_Common->AddSpell(SpellSlot::E, 50.f);
	Spells::R = g_Common->AddSpell(SpellSlot::R, 330.f);

	// pred hitchance
	Spells::Q->SetSkillshot(1.2f, 230.f, 20.f, kCollidesWithNothing, kSkillshotCircle);
	Spells::W->SetSkillshot(0.641f, 210.f, 4000, kCollidesWithNothing, kSkillshotCone);

	EventHandler<Events::GameUpdate>::AddEventHandler(OnGameUpdate);
	EventHandler<Events::OnAfterAttackOrbwalker>::AddEventHandler(OnAfterAttack);
	EventHandler<Events::OnHudDraw>::AddEventHandler(OnHudDraw);
	//	EventHandler<Events::OnBuff>::AddEventHandler(OnBuffChange);

	g_Common->ChatPrint("<font color='#FFC300'>Cho'Gath Loaded!</font>");
	g_Common->Log("Cho'Gath plugin loaded.");

	return true;
}

PLUGIN_API void OnUnloadSDK()
{
	Menu::MenuInstance->Remove();
	EventHandler<Events::GameUpdate>::RemoveEventHandler(OnGameUpdate);
	EventHandler<Events::GameUpdate>::RemoveEventHandler(OnGameUpdate);
	EventHandler<Events::OnAfterAttackOrbwalker>::RemoveEventHandler(OnAfterAttack);
	EventHandler<Events::OnHudDraw>::RemoveEventHandler(OnHudDraw);
	//	EventHandler<Events::OnBuff>::RemoveEventHandler(OnBuffChange);
	g_Common->ChatPrint("<font color='#00BFFF'>Cho'Gath Unloaded.</font>");
}

//changelog
//changed line 500/501 prediction, 501 changed to cone