#ifndef testH
#define testH

#include <global_string.h>
#include <memory>
#include <container/buffer.h>

#include "../../shared/protocol.h"

namespace Test
{

	class Test
	{
	public:
		const String	name;

		/**/			Test(const String&name) :name(name)
		{}


		virtual void	Run(Random&random) = 0;
	};


	typedef std::shared_ptr<Test>	PTest;
	
	extern Buffer0<PTest>	all;

	void Run(Test&test);



	class Serialization : public Test
	{
	public:
		/**/			Serialization() :Test("Serialization")
		{}

		virtual void	Run(Random&random)	override;

	};

	class StorageUnits : public Test
	{
	public:
		/**/			StorageUnits() :Test("StorageUnits")
		{}

		virtual void	Run(Random&random)	override;

	};

	class VehiclePlant : public Test
	{
	public:
		/**/			VehiclePlant() :Test("VehiclePlant")
		{}

		virtual void	Run(Random&random)	override;
		static void		EnqueueRandom(Objects::PVehiclePlant&plant, Random&random);

	};

	class SkyPort : public Test
	{
	public:
		/**/			SkyPort() :Test("SkyPort")
		{}

		virtual void	Run(Random&random)	override;
		static void		EnqueueRandom(Objects::PSkyPort&plant, Random&random);

	};

	class Bin2Str : public Test
	{
	public:
		/**/			Bin2Str() :Test("Bin2Str")
		{}

		static void		Check(const CryptographicID&);
		virtual void	Run(Random&random)	override;
	};

	class AutoTargeting : public Test
	{
	public:
		/**/			AutoTargeting() :Test("AutoTargeting")
		{}

		static Grid::LCellIdI	RandomCell(Random&random);
		static Grid::LCellIdI	RandomCellAtRange(Random&random,const Grid::LCellIdI&, const TFloatRange<>&);
		

		virtual void	Run(Random&random)	override;

	};


	class TechniteRandom : public Test
	{
	public:
		/**/			TechniteRandom(): Test("Technite Random")
		{}

		virtual void	Run(Random&random) override;

	};
	
}



#endif
