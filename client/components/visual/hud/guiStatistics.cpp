#include "guiStatistics.h"
#include <objects/statistics.h>
#include <objects/faction.h>
#include <objects/prototypes.h>
#include "../../network.h"
#include "windowStack.h"

namespace HUD
{
	namespace Statistics
	{
		using namespace Component;

		PWindow			window;
		PPanel			factionPanel,
						metricPanel,
						statsPanel;
		PMultiPlot			overviewGraph;
		//Buffer0<PButton>	overviewMetrics, overviewFactions;
		PointerTable<PButton>	factionButtonMap;
		PCheckbox				excludeNexusFromOverview;

		#if 0
		overview:
		+---------------+
		|	overview	|
		+-----------+---+---------------------------+-----------+
		| [metric0]	|								| [user0]	|
		| [metric1]	|						-----	| [user1]	|
		| [metric2]	|					----		| [user2]	|
		| [metric3]	|		-		----			| [user3]	|
		| [metric4]	|	---	 ------					| [user4]	|
		|			|---							|			|
		|			+-------------------------------+			|
		|			|				stats			|			|
		|			|								|			|
		+-----------+-------------------------------+-----------+
			


		#endif /*0*/



		typedef std::function<UINT64(const Objects::Statistics::Round&)> FFilter;

		static Objects::WFaction	chosenFaction;
		static FFilter				lastFilter;
		static index_t				lastFilterID = InvalidIndex;
		static bool					integrateFilter = false;
		static IndexTable<PButton>	filterButtons;
		static UINT32				endRound=0;

		static const float linePadding = 0.01f;

		static void AddPlot(const Objects::PFaction&f)
		{
			const auto&stat = f->statistics;
			overviewGraph->NextPlot(f->color);

			float last = 0;
			stat.VisitRounds([&last](const Objects::Statistics::Round&round)
			{
				float current = lastFilter(round);
				if (integrateFilter)
				{
					last += current;
					current = last;
				}
				overviewGraph->AddNode(round.mainRound,current);
			});
		}


		namespace Stats
		{

			bool left = true;

			static void Reset()
			{
				statsPanel->ClearComponents();
				left = true;
			}

			static void AddRow(const PComponent&c)
			{
				{
					auto mod = c->Anchor();
					if (left)
					{
						mod.FillHorizontally(0,Distance(0.5f,Distance::Type::RelativeToParentComponent));
						if (statsPanel->GetReferences().IsNotEmpty())
							mod.Beneath(statsPanel->GetReferences().Last(),0);
						else
							mod.Top(0.03f);
						if (statsPanel->CountComponents() > 1)
							mod.GetVAxis().upper.AddReference( statsPanel->GetReferences().GetFromEnd(1) );
					}
					else
					{
						mod.FillHorizontally(Distance(0.5f,Distance::Type::RelativeToParentComponent),0);
						if (statsPanel->CountComponents() > 1)
							mod.Beneath(statsPanel->GetReferences().GetFromEnd(1),0);
						else
							mod.Top(0.03f);
						if (statsPanel->CountComponents() > 2)
							mod.GetVAxis().upper.AddReference(statsPanel->GetReferences().GetFromEnd(2) );
					}
				}
				statsPanel->AddComponent(c,CLOCATION);
				left = !left;
			}

			static void AddLine(const String&line)
			{
				PLabel caption{new Label{line}};
				caption->alignCenter = false;
				//caption->SetVerticalPadding(linePadding);
				caption->SetPadding(0);
				AddRow(caption);
			}

			static void AddUserLine(const String&prefix,const Objects::PFaction&f,const String&postFix="")
			{
				PPanel panel{new Panel{}};
				panel->SetBackgroundTransparent();
				panel->SetAutomaticHeight(true);

				PLabel prefixLabel{new Label{prefix+' '}};
				prefixLabel->alignCenter = false;
				prefixLabel->Anchor().Top().Left();
				prefixLabel->LockToIdealHeight();
				prefixLabel->LockToIdealWidth();
				//prefixLabel->SetVerticalPadding(linePadding);
				prefixLabel->SetPadding(0.f);
				prefixLabel->LockToIdealWidth();
				panel->AddComponent(prefixLabel,CLOCATION);
			
				PLabel factionLabel{new Label{f->GetName()}};
				factionLabel->alignCenter = false;
				factionLabel->SetColor(f->color);
				factionLabel->Anchor().Top().RightOf(prefixLabel);
				//factionLabel->SetVerticalPadding(linePadding);
				factionLabel->SetPadding(0.f);
				factionLabel->LockToIdealWidth();
				panel->AddComponent(factionLabel,CLOCATION);
			
				if (postFix.IsNotEmpty())
				{
					PLabel postFixLabel{new Label{' '+postFix}};
					postFixLabel->Anchor().Top().RightOf(factionLabel);
					postFixLabel->alignCenter = false;
					//postFixLabel->SetVerticalPadding(linePadding);
					postFixLabel->SetPadding(0.f);
					postFixLabel->LockToIdealWidth();
					panel->AddComponent(postFixLabel,CLOCATION);
				}
				AddRow(panel);
			}

			static void AddUserLine(const String&prefix,const Objects::PAlliance&a,const String&postFix="")
			{
				PPanel panel{new Panel{}};
				panel->SetBackgroundTransparent();
				panel->SetAutomaticHeight(true);

				PLabel prefixLabel{new Label{prefix+' '}};
				prefixLabel->alignCenter = false;
				prefixLabel->Anchor().Top().Left();
				prefixLabel->LockToIdealHeight();
				prefixLabel->LockToIdealWidth();
				//prefixLabel->SetVerticalPadding(linePadding);
				prefixLabel->SetPadding(0.f);
				prefixLabel->LockToIdealWidth();
				panel->AddComponent(prefixLabel,CLOCATION);
			
				auto last = prefixLabel;
	
				a->VisitMembers([&last,panel](const Objects::PFaction&f)
				{
					PLabel factionLabel{new Label{f->GetName()+' '}};
					factionLabel->alignCenter = false;
					factionLabel->SetColor(f->color);
					factionLabel->Anchor().Top().RightOf(last);
					factionLabel->SetPadding(0.f);
					factionLabel->LockToIdealWidth();
					panel->AddComponent(factionLabel,CLOCATION);

					last = factionLabel;
				});
			
				if (postFix.IsNotEmpty())
				{
					PLabel postFixLabel{new Label{postFix}};
					postFixLabel->Anchor().Top().RightOf(last);
					postFixLabel->alignCenter = false;
					//postFixLabel->SetVerticalPadding(linePadding);
					postFixLabel->SetPadding(0.f);
					postFixLabel->LockToIdealWidth();
					panel->AddComponent(postFixLabel,CLOCATION);
				}
				AddRow(panel);
			}

		}

		struct TFactionStats
		{
			UINT64 totalEntitiesKilled = 0,
					totalEntitiesLost = 0, 
					totalEntitiesProduced = 0, 
					totalDamageDealt = 0, 
					highestOffensiveCapabilities = 0,
					mostTerraformers = 0,
					mostFactories = 0,
					totalDamageReceived = 0;

			UINT64	totalMobilesBuilt = 0;
			UINT64	totalStructuresBuilt = 0;
		};

		static TFactionStats GetFactionStats(const Objects::PFaction&f)
		{
			TFactionStats rs;
			f->statistics.VisitRounds([&](const Objects::Statistics::Round&r)
			{
				typedef Objects::Statistics::Metric M;
				rs.totalEntitiesProduced += r.metrics[M::UnitsBuilt];
				rs.totalEntitiesLost += r.metrics[M::UnitsKilledByOther] + r.metrics[M::UnitsStolenByOther];
				rs.totalEntitiesKilled += r.metrics[M::UnitsKilled];
				rs.totalDamageDealt += r.metrics[M::DamageDealt] + r.metrics[M::EMPDamageDealt];
				rs.totalDamageReceived += r.metrics[M::DamageReceived] + r.metrics[M::EMPDamageReceived];

				rs.highestOffensiveCapabilities = std::max(rs.highestOffensiveCapabilities,r.metrics[M::OffensiveCapabilities]);
				rs.mostTerraformers = std::max(rs.mostTerraformers,r.metrics[M::NumTerraformers]);
				rs.mostFactories = std::max(rs.mostFactories,r.metrics[M::NumFactories]);
			});



			f->statistics.VisitEntityTypes([&](const Objects::Statistics::EntityType&t)
			{
				typedef Objects::Statistics::Metric M;
				bool isMobile = false;
				if (t.type.IsVehicle() || !Objects::Prototypes::structures.Find(t.type))
					isMobile = true;

				if (isMobile)
					rs.totalMobilesBuilt += t.metrics[M::UnitsBuilt];
				else
					rs.totalStructuresBuilt += t.metrics[M::UnitsBuilt];
			});

			return rs;
		}


		struct TVictor
		{
			UINT64 score = 0;
			Objects::PFaction	faction;

			void	Check(const Objects::PFaction&other, UINT64 otherScore)
			{
				if (score >= otherScore)
					return;
				score = otherScore;
				faction = other;
			}
		};

		static void BuildGeneralStatistics()
		{
			{
				String title = "Match Overview";
				PLabel caption{new Label{title,1.2f}};
				statsPanel->AnchorBelowPrevious(CLOCATION,caption);
			}

			Stats::AddLine("World: "+Network::worldIDString);
			Stats::AddLine("Participants: "+String(Objects::Faction::allKnown.Count()));


			Stats::AddLine("Total rounds: "+String(endRound));

			Objects::PAlliance winner = Objects::Alliance::FindByID(Network::winningAllianceID);
			if (winner)
				Stats::AddUserLine("Winner:",winner,"");

			TVictor	warlord, punchingBag, largestArmy, mostTerraformers, mostStructuresBuilt, mostMobilesBuilt;
			Objects::ForeachFaction([&](const Objects::PFaction&f)
			{
				const TFactionStats stats= GetFactionStats(f);
				warlord.Check(f,stats.totalDamageDealt);
				punchingBag.Check(f,stats.totalDamageReceived);
				largestArmy.Check(f,stats.highestOffensiveCapabilities);
				mostTerraformers.Check(f,stats.mostTerraformers);
				mostStructuresBuilt.Check(f,stats.totalStructuresBuilt);
				mostMobilesBuilt.Check(f,stats.totalMobilesBuilt);
			});
			if (warlord.faction)
				Stats::AddUserLine("Most inflicted:",warlord.faction);
			if (punchingBag.faction)
				Stats::AddUserLine("Most lost:",punchingBag.faction);
			if (largestArmy.faction)
				Stats::AddUserLine("Biggest arsenal:",largestArmy.faction);
			if (mostTerraformers.faction)
				Stats::AddUserLine("Most Terraformers:",mostTerraformers.faction);
			if (mostStructuresBuilt.faction)
				Stats::AddUserLine("Most buildings built:",mostStructuresBuilt.faction);
			if (mostMobilesBuilt.faction)
				Stats::AddUserLine("Most mobiles built:",mostMobilesBuilt.faction);
		}

		static void BuildFactionStatistics(const Objects::PFaction&f)
		{
			{
				String title = f->GetName();
				if (f->alliance && Network::winningAllianceID == f->alliance->id)
					title += " (WINNER)";
				PLabel caption{new Label{title,1.2f}};
				caption->SetColor(f->color);
				//caption->alignCenter = false;
				statsPanel->AnchorBelowPrevious(CLOCATION,caption);
			}

			const auto fStats = GetFactionStats(f);


			Stats::AddLine("First round: "+String(f->statistics.GetStartingRound()));
			Stats::AddLine("Final round: "+String(f->statistics.GetFinalRound()) + (f->statistics.GetFinalRound() == endRound ? " (game end)":""));

			Stats::AddLine("Mobiles produced: "+String(fStats.totalMobilesBuilt));
			Stats::AddLine("Structures built: "+String(fStats.totalStructuresBuilt));
			Stats::AddLine("Units destroyed: "+String(fStats.totalEntitiesKilled));
			Stats::AddLine("Damage dealt: "+String(fStats.totalDamageDealt));
			Stats::AddLine("Units lost: "+String(fStats.totalEntitiesLost));
			Stats::AddLine("Damage received: "+String(fStats.totalDamageReceived));

				
			Objects::PFaction nemesis,prey;

			UINT64 damageNemesis=0,damagePrey =0;

			f->statistics.VisitFactionInteractions([&](const Objects::Statistics::FactionInteraction&inter)
			{
				typedef Objects::Statistics::Metric M;
				const UINT64 dealt = inter.metrics[M::DamageDealt];
				const UINT64 received = inter.metrics[M::DamageReceived];
				if (dealt > damagePrey)
				{
					damagePrey = dealt;
					prey = Objects::Faction::FindByID(inter.remoteID);
				}
				if (received > damageNemesis)
				{
					damageNemesis = received;
					nemesis = Objects::Faction::FindByID(inter.remoteID);
				}
			});

			if (nemesis && nemesis == prey)
			{
				if (damageNemesis > damagePrey)
					prey.reset();
				else
					nemesis.reset();
			}

			if (prey)
				Stats::AddUserLine("Prey:",prey/*,"("+String(damagePrey)+" damage dealt)"*/);
			if (nemesis)
				Stats::AddUserLine("Nemesis:",nemesis/*,"("+String(damageNemesis)+" damage received)"*/);
		}

		static void RebuildStackedGraphAndStats()
		{
			{
				overviewGraph->ClearPlots();
				if (!chosenFaction.expired())
				{
					AddPlot(chosenFaction.lock());
				}
				else
				{
					const bool excludeNexus = excludeNexusFromOverview->IsChecked();
					Objects::ForeachFaction([excludeNexus](const Objects::PFaction&f)
					{
						if (!excludeNexus || f->name != Objects::Faction::nexusName)
							AddPlot(f);
					},true);
				}
			}



			Stats::Reset();
			if (!chosenFaction.expired())
				BuildFactionStatistics(chosenFaction.lock());
			else
				BuildGeneralStatistics();
		}

		static PButton AddStackFilter(const String&name, const FFilter&filter, bool trigger=false, bool integrate=false)
		{
			PButton button{new Button{name}};
			static index_t idCounter = 0;
			const index_t myID = idCounter++;

			filterButtons.Set(myID,button);

			button->SetVerticalPadding(0.5f);
			metricPanel->AnchorBelowPrevious(CLOCATION,button,0.005f);
			WButton w = button;
			button->SetTriggerFunction([filter,w,myID,integrate]()
			{
				PButton button = w.lock();
				if (!button)
					return;
				if (button->IsAlwaysDown())
					return;

				if (lastFilterID != InvalidIndex)
				{
					PButton other = filterButtons.Require(lastFilterID);
					other->SetAlwaysDown(false);
				}
				button->SetAlwaysDown(true);

				lastFilterID = myID;
				lastFilter = filter;
				integrateFilter = integrate;
				RebuildStackedGraphAndStats();

			});

			if (trigger)
				button->GetTriggerFunction()();

			return button;
		}

		static PButton AddProduction(const String&name, const Objects::Resource res)
		{
			return AddStackFilter(name, [res](const Objects::Statistics::Round&r){return r.production.level[res];},false);
		}

		static PButton AddMetric(const String&name, const Objects::Statistics::Metric m, bool trigger = false, bool integrate=false)
		{
			return AddStackFilter(name, [m](const Objects::Statistics::Round&r){return r.metrics[m];},trigger,integrate);
		}

		void	Update()
		{
			endRound = 0;
			Objects::ForeachFaction([](const Objects::PFaction&f)
			{
				endRound = M::Max(endRound,f->statistics.GetFinalRound());
			},true);



			if (!window)
			{
				window = Window::Create();
				window->SetAutomaticHeight(false);
				window->SetWidth(0.75f);
				window->SetHeight(0.75f);

				excludeNexusFromOverview.reset(new Checkbox{"Exclude Nexus from overview",true,[](bool)
				{
					RebuildStackedGraphAndStats();
				}});

				overviewGraph.reset(new MultiPlot{});

				metricPanel.reset(new Panel{});
				factionPanel.reset(new Panel{});
				statsPanel.reset(new Panel{});

				statsPanel->SetBackgroundTransparent();
				factionPanel->SetBackgroundTransparent();
				metricPanel->SetBackgroundTransparent();

				window->AddComponent(factionPanel,CLOCATION);
				window->AddComponent(metricPanel,CLOCATION);
				window->AddComponent(overviewGraph,CLOCATION);
				window->AddComponent(statsPanel,CLOCATION);


				metricPanel->SetAutomaticHeight(false);
				metricPanel->Anchor().Left().FillVertically();
				metricPanel->SetWidth(0.15f);

				factionPanel->SetAutomaticHeight(false);
				factionPanel->Anchor().Right().FillVertically();
				factionPanel->SetWidth(0.15f);

				statsPanel->SetAutomaticHeight(false);
				statsPanel->Anchor().FillHBetween(metricPanel,factionPanel).Bottom();
				statsPanel->SetHeight(0.25f);

				{
					auto anchors = overviewGraph->Anchor();
					anchors.FillHBetween(metricPanel,factionPanel);
					anchors.GetVAxis().upper.Set(0);
					anchors.GetVAxis().lower.Set(statsPanel, 0);
					anchors.GetVAxis().isAutoCentering = false;
				}


				{
					AddMetric("Score", Objects::Statistics::Metric::WinningScore, true);
					AddMetric("Number of Units", Objects::Statistics::Metric::CurrentEntityCount);
					AddMetric("Value of Units", Objects::Statistics::Metric::CurrentEntityMineralValue);
					AddMetric("Sensor Coverage", Objects::Statistics::Metric::CurrentSensorCoverage);
					AddMetric("Offensive strength", Objects::Statistics::Metric::OffensiveCapabilities);
					AddMetric("Units destroyed", Objects::Statistics::Metric::UnitsKilled,false,true);
					AddMetric("Units lost", Objects::Statistics::Metric::UnitsKilledByOther,false,true);
					AddMetric("Damage inflicted", Objects::Statistics::Metric::DamageDealt,false,true);
					AddMetric("Damage suffered", Objects::Statistics::Metric::DamageReceived,false,true);
					AddMetric("Resources destroyed", Objects::Statistics::Metric::ResourcesDestroyed,false,true);
					AddMetric("Nexus Ports owned", Objects::Statistics::Metric::NexusPortsOwned);
					AddMetric("Nexus Ports controlled", Objects::Statistics::Metric::NexusPortsControlled);
					AddMetric("Research speed", Objects::Statistics::Metric::ResearchSpeed);
					AddMetric("Terraformers", Objects::Statistics::Metric::NumTerraformers);
					AddProduction("Mineral Output",Objects::Resource::Minerals);
					AddProduction("Fuel Output",Objects::Resource::Fuel);
					AddProduction("Uranium Output",Objects::Resource::Uranium);
					AddProduction("Energy Output",Objects::Resource::Energy);
				}
			}


			{
				Objects::PFaction f = chosenFaction.lock();
				if (f && !Objects::Faction::allKnown.Contains(f))
					chosenFaction.reset();
			}

			factionPanel->ClearComponents();
			factionPanel->AnchorBelowPrevious(CLOCATION,excludeNexusFromOverview);
			factionButtonMap.Clear();
			Objects::ForeachFaction([](const Objects::PFaction&f)
			{
				PButton button{new Button{f->GetName()}};
				factionButtonMap.Set(f.get(),button);
				button->SetColor(f->color);
				Objects::WFaction f2 = f;
				WButton b2 = button;
				button->SetTriggerFunction([f2,b2]()
				{
					auto button = b2.lock();
					if (!button)
						return;
					if (button->IsAlwaysDown())
						chosenFaction.reset();
					else
					{
						Objects::PFaction old = chosenFaction.lock();
						if (old)
						{
							PButton other = factionButtonMap.Require(old.get());
							other->SetAlwaysDown(false);
						}
						chosenFaction = f2.lock();
					}
					button->SetAlwaysDown(!button->IsAlwaysDown());
					RebuildStackedGraphAndStats();
				});
				factionPanel->AnchorBelowPrevious(CLOCATION,button,0.005f);
				if (f == chosenFaction.lock())
					button->Trigger();
			},true);

			RebuildStackedGraphAndStats();
		}

		void Show()
		{
			WindowStack::Show(window);
		}

		void Hide()
		{
			WindowStack::Hide(window);
		}
	}

}