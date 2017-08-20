#include "guiStatistics.h"
#include <objects/statistics.h>
#include <objects/faction.h>
#include <objects/prototypes.h>
#include "../network.h"

namespace GUI
{
	namespace Statistics
	{
		PWindow			window;
		PPanel			factionPanel,
						metricPanel,
						statsPanel;
		PPlotStack			overviewGraph;
		//Buffer0<PButton>	overviewMetrics, overviewFactions;
		PointerTable<PButton>	factionButtonMap;

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
		static IndexTable<PButton>	filterButtons;
		static UINT32				endRound=0;

		static const float linePadding = 0.01f;

		static void AddPlot(const Objects::PFaction&f)
		{
			const auto&stat = f->statistics;
			PlotStack::Plot&g = overviewGraph->AddPlot(f->color);

			stat.VisitRounds([&g](const Objects::Statistics::Round&round)
			{
				g.AddNode(round.mainRound,lastFilter(round));
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
				auto mod = c->ModifyAnchorConfig();
				if (left)
				{
					mod.Left().Set(0);
					mod.Top().Set(statsPanel->GetComponents().Last(),0);
					if (statsPanel->CountComponents() > 1)
						mod.Top().references << statsPanel->GetComponents().GetFromEnd(1);
					mod.Right().Set(0.5f);
				}
				else
				{
					mod.Left().Set(0.5f);
					mod.Top().Set(statsPanel->GetComponents().GetFromEnd(1),0);
					if (statsPanel->CountComponents() > 2)
						mod.Top().references << statsPanel->GetComponents().GetFromEnd(2);
					mod.Right().Set(0);
				}

				statsPanel->AddComponent(c);
				left = !left;
			}

			static void AddLine(const String&line)
			{
				PLabel caption{new Label{line}};
				caption->alignCenter = false;
				caption->SetVerticalPadding(linePadding);
				AddRow(caption);
			}

			static void AddUserLine(const String&prefix,const Objects::PFaction&f,const String&postFix)
			{
				PPanel panel{new Panel{}};
				panel->fillBackground = false;
				panel->SetAutomaticHeight(true);

				PLabel prefixLabel{new Label{prefix+' '}};
				prefixLabel->alignCenter = false;
				prefixLabel->LockToIdealHeight();
				prefixLabel->LockToIdealWidth();
				prefixLabel->ModifyAnchorConfig().Top().Set(0);
				prefixLabel->ModifyAnchorConfig().Left().Set(0);
				prefixLabel->SetVerticalPadding(linePadding);
				panel->AddComponent(prefixLabel);
			
				PLabel factionLabel{new Label{f->GetName()}};
				factionLabel->alignCenter = false;
				factionLabel->SetColor(f->color);
				factionLabel->ModifyAnchorConfig().Top().Set(0);
				factionLabel->ModifyAnchorConfig().Left().Set(prefixLabel,0);
				factionLabel->SetVerticalPadding(linePadding);
				panel->AddComponent(factionLabel);
			
				if (postFix.IsNotEmpty())
				{
					PLabel postFixLabel{new Label{' '+postFix}};
					postFixLabel->ModifyAnchorConfig().Top().Set(0);
					postFixLabel->ModifyAnchorConfig().Left().Set(factionLabel,0);
					postFixLabel->alignCenter = false;
					postFixLabel->SetVerticalPadding(linePadding);
					panel->AddComponent(postFixLabel);
				}
				AddRow(panel);
			}

		}


		static void BuildFactionStatistics(const Objects::PFaction&f)
		{
			{
				String title = f->GetName();
				if (Network::winningFactionID == f->id)
					title += " (WINNER)";
				PLabel caption{new Label{title,1.2f}};
				caption->SetColor(f->color);
				//caption->alignCenter = false;
				statsPanel->AnchorBelowPrevious(caption);
			}

			UINT64 totalEntitiesKilled = 0, totalEntitiesLost = 0, totalEntitiesProduced = 0, totalDamageDealt = 0, totalDamageReceived = 0;
			f->statistics.VisitRounds([&](const Objects::Statistics::Round&r)
			{
				typedef Objects::Statistics::Metric M;
				totalEntitiesProduced += r.metrics[M::UnitsBuilt];
				totalEntitiesLost += r.metrics[M::UnitsKilledByOther] + r.metrics[M::UnitsStolenByOther];
				totalEntitiesKilled += r.metrics[M::UnitsKilled];
				totalDamageDealt += r.metrics[M::DamageDealt] + r.metrics[M::EMPDamageDealt];
				totalDamageReceived += r.metrics[M::DamageReceived] + r.metrics[M::EMPDamageReceived];
			});

			UINT64 totalMobilesBuilt = 0;
			UINT64 totalStructuresBuilt = 0;

			f->statistics.VisitEntityTypes([&](const Objects::Statistics::EntityType&t)
			{
				typedef Objects::Statistics::Metric M;
				bool isMobile = false;
				if (t.type.IsVehicle() || !Objects::Prototypes::structures.Find(t.type))
					isMobile = true;

				if (isMobile)
					totalMobilesBuilt += t.metrics[M::UnitsBuilt];
				else
					totalStructuresBuilt += t.metrics[M::UnitsBuilt];

			});

			Stats::AddLine("First round: "+String(f->statistics.GetStartingRound()));
			Stats::AddLine("Final round: "+String(f->statistics.GetFinalRound()) + (f->statistics.GetFinalRound() == endRound ? " (game end)":""));

			Stats::AddLine("Mobiles produced: "+String(totalMobilesBuilt));
			Stats::AddLine("Structures built: "+String(totalStructuresBuilt));
			Stats::AddLine("Units destroyed: "+String(totalEntitiesKilled));
			Stats::AddLine("Damage dealt: "+String(totalDamageDealt));
			Stats::AddLine("Units lost: "+String(totalEntitiesLost));
			Stats::AddLine("Damage received: "+String(totalDamageReceived));

				
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
				Stats::AddUserLine("Prey:",prey,"("+String(damagePrey)+" damage dealt)");
			if (nemesis)
				Stats::AddUserLine("Nemesis:",nemesis,"("+String(damageNemesis)+" damage received)");
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
					Objects::ForeachFaction([](const Objects::PFaction&f)
					{
						AddPlot(f);
					},true);
			}



			Stats::Reset();
			if (!chosenFaction.expired())
			{
				BuildFactionStatistics(chosenFaction.lock());

			}

		}

		static PButton AddStackFilter(const String&name, const FFilter&filter, bool trigger=false)
		{
			PButton button{new Button{name}};
			static index_t idCounter = 0;
			const index_t myID = idCounter++;

			filterButtons.Set(myID,button);

			metricPanel->AnchorBelowPrevious(button,0.005f);
			WButton w = button;
			button->SetTriggerFunction([filter,w,myID]()
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

		static PButton AddMetric(const String&name, const Objects::Statistics::Metric m, bool trigger = false)
		{
			return AddStackFilter(name, [m](const Objects::Statistics::Round&r){return r.metrics[m];},trigger);
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
				window = GUI::Window::Create();
				window->SetAutomaticHeight(false);
				window->SetWidth(0.75f);
				window->SetHeight(0.75f);


				overviewGraph.reset(new PlotStack{});

				metricPanel.reset(new Panel{});
				factionPanel.reset(new Panel{});
				statsPanel.reset(new Panel{});

				statsPanel->fillBackground = false;
				factionPanel->fillBackground = false;
				metricPanel->fillBackground = false;

				window->AddComponent(factionPanel);
				window->AddComponent(metricPanel);
				window->AddComponent(overviewGraph);
				window->AddComponent(statsPanel);

				{
					auto anchors = metricPanel->ModifyAnchorConfig();
					anchors.Left().Set(0);
					anchors.Top().Set(0);
					anchors.Bottom().Set(0);
				}
				metricPanel->SetWidth(0.15f);
				metricPanel->SetAutomaticHeight(false);

				{
					auto anchors = factionPanel->ModifyAnchorConfig();
					anchors.Right().Set(0);
					anchors.Top().Set(0);
					anchors.Bottom().Set(0);
				}
				factionPanel->SetWidth(0.15f);
				factionPanel->SetAutomaticHeight(false);
				{
					auto anchors = statsPanel->ModifyAnchorConfig();
					anchors.Left().Set(metricPanel,0);
					anchors.Right().Set(factionPanel,0);
					anchors.Bottom().Set(0);
				}
				statsPanel->SetAutomaticHeight(false);
				statsPanel->SetHeight(0.25f);

				{
					auto anchors = overviewGraph->ModifyAnchorConfig();
					anchors.Right().Set(factionPanel,0);
					anchors.Left().Set(metricPanel,0);
					anchors.Top().Set(0);
					anchors.Bottom().Set(statsPanel,0);
				}


				{
					AddMetric("Number of Units", Objects::Statistics::Metric::CurrentEntityCount);
					AddMetric("Value of Units", Objects::Statistics::Metric::CurrentEntityMineralValue,true);
					AddMetric("Sensor Coverage", Objects::Statistics::Metric::CurrentSensorCoverage);
					AddMetric("Units destroyed", Objects::Statistics::Metric::UnitsKilled);
					AddMetric("Units lost", Objects::Statistics::Metric::UnitsKilledByOther);
					AddMetric("Damage Dealt", Objects::Statistics::Metric::DamageDealt);
					AddMetric("Damage Received", Objects::Statistics::Metric::DamageReceived);
					AddMetric("Resources Destroyed", Objects::Statistics::Metric::ResourcesDestroyed);
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
				factionPanel->AnchorBelowPrevious(button,0.005f);
				if (f == chosenFaction.lock())
					button->Trigger();
			},true);

			RebuildStackedGraphAndStats();
		}
	}
}