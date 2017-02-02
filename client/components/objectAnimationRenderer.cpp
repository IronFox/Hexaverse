#include <global_root.h>
#include "objectAnimationRenderer.h"
#include <objects.h>

namespace Objects
{

	static Engine::DX11::Texture	Render(const PWorldEntity&e)
	{
		float distanceFactor = 1.f;
		if (e->IsMobile())
		{
			#ifndef LOWER_RESOLUTION_PLANET
				distanceFactor *= 0.5f;
			#endif
		}
		if (e->IsStructure())
			distanceFactor += 0.1f * To<Structure>::Cast(e)->GetHeight();
		#ifndef NO_SATELLITES
			if (e->IsSatellite())
				distanceFactor *= 3.f;
		#endif
		GeometryImage::BeginRendering(float4(0),distanceFactor);

		keyColor.Update(float4(0.5,0.5,1,1));

		Renderer::RenderSimple(*e);

		return GeometryImage::EndRendering();
	}


	static void	FinishRendering(const PathString&outName, const DX11::Texture&texture)
	{
		static Image image;
		display.Copy(texture.GetHandle2D(),image);
					
		image.ScaleHalf();
		image.ScaleHalf();
		//image.TruncateToOpaque();

		png.SaveToFileQ(image, outName.c_str());
	}
}


static void RenderAtAngle(const FileSystem::Folder&folder, const Objects::PWorldEntity&e, int angle, float xLen, float yLen, float zLen)
{
	PathString outName = PathString(angle)+".png";
	while (outName.length() < 7)
		outName = '0'+outName;

	PathString writeTo = folder.FindFile(outName,false)->GetLocation();
	if (FileSystem::IsFile(writeTo))
		return;

	TMatrix3<> mat;
	Mat::FillZRotationMatrix(angle,mat);
	mat.x *= xLen;
	mat.y *= yLen;
	mat.z *= zLen;
	Mat::copyOrientation(mat,e->system);

	Objects::FinishRendering(writeTo,Objects::Render(e));
}


PathString MakeValidFilename(String base);


static void Render(const FileSystem::Folder&folder, const Objects::PWorldEntity&e)
{
	LogEvent(EventSignificance::Uncommon, "Capturing "+e->GetTypeName()+"...");
	FileSystem::Folder sub(folder.FindFolder(MakeValidFilename(e->GetTypeName()),false));
	if (!sub.IsValidLocation())
	{
		FileSystem::CreateDirectory(sub.GetLocation());
	}


	StringFile file;
	file.Create(sub.FindFile("desc.txt",false)->GetLocation());

	Objects::BaseCommand::Label label;
	e->DescribeType(label,0xFFFFFFFF & ~(Objects::WorldEntity::DescIncludeCurrentHealth | Objects::WorldEntity::DescIncludeUnitCount));


	foreach (label.rawLines,line)
	{
		for (index_t i = 0; i<Objects::Resource::N; i++)
			line->first.replace(Objects::ResourceTag(i),String(Objects::Resource::Reinterpret(i).ToString())+" ");
		line->first.replace(Objects::TimeResourceTag(),"Rounds ");
		line->first.replace(" ,",",");
		line->first.replace(" .",".");
		file << line->first << nl;
	}
	file.Close();

	float xLen = Vec::length(e->system.x);
	float yLen = Vec::length(e->system.y);
	float zLen = Vec::length(e->system.z);
	for (int i = 0; i < 360; i+=2)
	{
		RenderAtAngle(sub,e,i,xLen,yLen,zLen);
	}
	Mat::copyOrientation(Matrix<>::eye3,e->system);
	e->system.x *= xLen;
	e->system.y *= yLen;
	e->system.z *= zLen;
}

void ObjectAnimationRenderer::RenderTo(const FileSystem::Folder &folder)
{
	Objects::GeometryImage::resultResolution = 2048;

	Objects::ResearchState::ActivateProgram(Objects::ResearchState::globalState.factoryDefault);
	
	foreach (Objects::structurePrototypes,s)
	{
		Render(folder,*s);
	}

	foreach (Objects::Component::baseComponents,b)
	{
		foreach (Objects::Component::allTurretComponents,c)
		{
			if ((*c)->mountType == Objects::Component::Turret::MountType::Vehicle)
			{
				Objects::PVehicle v(new Objects::Vehicle(Objects::OwnerParameter(),Grid::LCellIdI(),*b,*c));
				Render(folder,v);
			}
		}
	}

	foreach (Objects::SubOrbital::prototypes,p)
		Render(folder,*p);
}
