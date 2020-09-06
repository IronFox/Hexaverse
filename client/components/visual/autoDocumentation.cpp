#include "autoDocumentation.h"
#include "hud/dialog.h"
#include <logEvent.h>
#include "scene/objectRendering.h"
#include "hud/objectCommands.h"
#include <objects/prototypes.h>
#include <objects/entity.h>
#include <objects/structure.h>
#include <image/converter/png.h>
#include "../../ExtRenderer/DirectX11.h"

namespace AutoDocumentation
{
	using namespace DeltaWorks;
	using namespace Engine;

	
	bool IsValidFileChar(char c)
	{
		if (CharFunctions::isalnum(c))
			return true;
		switch (c)
		{
		case ' ':
		case '-':
			return true;
		}
		return false;
	}

	PathString MakeValidFilename(String base)
	{
		base.EraseCharacters(IsValidFileChar,false);
		return PathString(base);
	}


	void WriteLabel(StringFile&file, const Objects::Description&label, const String&myTypeName)
	{


		foreach(label, line)
		{
			if (line->first.GetLength() == 0)
				continue;
			if (line->second)
				file << "*";
			String linkedLine = line->first;
			linkedLine.FindAndReplace("Hrz. Attack range","[[concepts/attack range|Hrz. Attack range]]");
			linkedLine.FindAndReplace("Base horizontal attack range","[[concepts/attack range|Base horizontal attack range]]");
			linkedLine.FindAndReplace("Initiative","[[concepts/initiative|Initiative]]");
			linkedLine.FindAndReplace("Weight","[[concepts/vehicle|Weight]]");
			linkedLine.FindAndReplace("Carrying capacity","[[concepts/vehicle|Carrying capacity]]");
//			linkedLine.FindAndReplace("Base scanning range","[[concepts/sensory|Base scanning range]]");
			linkedLine.FindAndReplace("scanning range","[[concepts/scanning range]]");
			linkedLine.FindAndReplace("Scanning range","[[concepts/scanning range|Scanning range]]");
			linkedLine.FindAndReplace("compound","[[concepts/compound]]");
			linkedLine.FindAndReplace("phase","[[concepts/phase]]");
			linkedLine.FindAndReplace("attack action","[[concepts/action pool|attack action]]");
			linkedLine.FindAndReplace("resources","[[concepts/resources]]");
			

			
			
			foreach (Objects::Prototypes::structures,s)
			{
				String name = (*s)->GetTypeName().CopyToLowerCase();
				if (name.EqualsIgnoreCase(myTypeName))
					continue;

				linkedLine.FindAndReplace(name,"[[units/structures/"+name+"]]");
			}



			foreach(linkedLine, c)
			{
				if (Objects::IsTag(*c))
				{
					file << "%"<<Objects::GetTagName(*c)<<"%";
					continue;
				}
				file << *c;
			}
			file << nl;
		}
	}

	void WriteDocumentation(const FileSystem::Folder&rootFolder, const FileSystem::Folder&outFolder, StringFile&listFile, const String&typeName, const Objects::Command::Base::Label&label, const String&pretext = "")
	{
		LogEvent(EventSignificance::Uncommon, "Documenting "+typeName);

		PathString name = MakeValidFilename( typeName );
		PathString outName = (name+".prefix").ConvertToLowerCase();
		StringFile file;
		if (!outFolder.IsValidLocation())
			FileSystem::CreateDirectory(outFolder.GetLocation());

		String relative = String(FileSystem::GetRelativePath(rootFolder.GetLocation(),outFolder.GetLocation()));
		relative.FindAndReplace('\\','/');
		PathString outPath = outFolder.FindFile(outName,false)->GetLocation();
		listFile << "* [[units/"<<relative<<'/'<<typeName<<'|'<<typeName <<"]]"<< nl;
		file.Create(outPath);

		file << "{{ img:units/"<< String(name) <<".png|"<<typeName<<"}}"<<nl;
		file << "="<<typeName<<"="<<nl;
		file << pretext;
		WriteLabel(file,label,typeName);
	}

	void	FinishRendering(const FileSystem::Folder&folder, const DX11::Texture&texture, const String&typeName)
	{
		static Image image;
		display.Copy(texture.GetHandle2D(),image);
					
		image.ScaleHalf();
		image.ScaleHalf();
		image.TruncateToOpaque();
		PathString outName = MakeValidFilename(typeName)+".png";

		png.SaveToFile(image, folder.FindFile(outName,false)->GetLocation().c_str());
	}

	void Run(FileSystem::Folder f)
	{
		ASSERT__(f.IsValidLocation());
		{
			FileSystem::Folder imageTarget = f.FindDirectory("images/units",false);
			if (!imageTarget.IsValidLocation())
				FileSystem::CreateDirectory(imageTarget.GetLocation());
			ASSERT__(imageTarget.IsValidLocation());

			{
				LogEvent(EventSignificance::Uncommon, "Rendering type images to "+String(imageTarget.GetLocation())+ "...");
				Objects::GeometryImage::resultResolution = 1024;
				Objects::Command::EntityCommand container;
				Image image;
				FileSystem::Folder folder(imageTarget);
				foreach (Objects::Prototypes::all,s)
				{
					LogEvent(EventSignificance::Uncommon, "Capturing "+(*s)->GetTypeName());
					Objects::Command::EntityCommand::UpdateTexture(container,*s,true);
					FinishRendering(folder, container.texture, (*s)->GetTypeName());
				}

				foreach (Objects::Component::allTurretComponents,c)
				{
					LogEvent(EventSignificance::Uncommon, "Capturing "+(*c)->typeName);
					Objects::Command::EntityConstructionCommand::UpdateTexture(container,nullptr,*c);
					FinishRendering(folder, container.texture, (*c)->typeName);
				}

				foreach (Objects::Component::baseComponents,c)
				{
					LogEvent(EventSignificance::Uncommon, "Capturing "+(*c)->typeName);
					Objects::Command::EntityConstructionCommand::UpdateTexture(container,*c,nullptr);
					FinishRendering(folder, container.texture, (*c)->typeName);
				}
			}
		}
		{
			FileSystem::Folder documentationTarget = f.FindDirectory("documentation",false);
			if (!documentationTarget.IsValidLocation())
				FileSystem::CreateDirectory(documentationTarget.GetLocation());

			ASSERT__(documentationTarget.IsValidLocation());

			{
				FileSystem::Folder rootFolder(documentationTarget.FindDirectory("units",false));
				FileSystem::Folder structureFolder(rootFolder.FindDirectory("structures",false));
				FileSystem::Folder turretFolder(rootFolder.FindDirectory("turrets",false));
				FileSystem::Folder vbaseFolder(rootFolder.FindDirectory("vbases",false));

				StringFile unitList;
				unitList.Create(rootFolder.FindFile("list.prefix",false)->GetLocation());

				unitList << "=Unit Types and Components="<<nl;

				unitList << "==Structures=="<<nl;
				foreach (Objects::Prototypes::structures,s)
				{
					Objects::Command::Base::Label label;
					(*s)->DescribeType(label,0xFFFFFFFF & ~Objects::WorldEntity::DescIncludeName & ~Objects::WorldEntity::DescIncludeCurrentHealth,nullptr,nullptr);
					String pretext;
					auto tb = (*s)->GetTurretBase();
					if (tb)
					{
						label.Insert(0).first = "Based on [[units/turrets/"+String(MakeValidFilename(tb->turret->typeName))+"|"+tb->turret->typeName+"]] turret";
						label.SetBrief(0);
					}
					WriteDocumentation(rootFolder, structureFolder,unitList, (*s)->GetTypeName(), label,pretext);
				}

				unitList << "==Turrets=="<<nl;
				foreach (Objects::Component::allTurretComponents,c)
				{
					Objects::Command::Base::Label label;
					(*c)->DescribeType(label,false,false,"",nullptr,nullptr);
					WriteDocumentation(rootFolder, turretFolder,unitList, (*c)->typeName, label);
				}

				unitList << "==Vehicle Bases=="<<nl;
				foreach (Objects::Component::baseComponents,c)
				{
					Objects::Command::Base::Label label;
					(*c)->DescribeType(label,false,false,nullptr,nullptr);
					WriteDocumentation(rootFolder, vbaseFolder,unitList, (*c)->typeName, label);
				}
			}
		}
	}


}
