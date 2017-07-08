#ifndef testH
#define testH

#include <global_string.h>
#include <memory>
#include <container/buffer.h>

#include "../../shared/protocol.h"

namespace Test
{
	typedef RNG<std::mt19937>	Random;

	class Exception : public Except::Program::GeneralFault
	{
		typedef Except::Program::GeneralFault Super;
	public:
		/**/		Exception(){}\
		/**/		Exception(const char*message):Super(message) {}\
		/**/		Exception(const Except::globalString&string):Super(string){}\
		/**/		Exception(const TCodeLocation&loc):Super(loc){}\
		/**/		Exception(const TCodeLocation&loc, const char*message):Super(loc,message) {}\
		/**/		Exception(const TCodeLocation&loc, const Except::globalString&string):Super(loc,string){}\
		template <class C>		Exception(const IString<C>&string):Super(string){}\
		template <class C>		Exception(const TCodeLocation&loc, const IString<C>&string):Super(loc,string){}\
		static const Except::Name&		StaticGetName()	{static const Except::Name name=Except::Name(Super::StaticGetName(),"/Test::Exception"); return name;}
		virtual	const char*		GetType()	const override					{return StaticGetName();}
	};

	class Test
	{
	public:
		const String	name;

		/**/			Test(const String&name) :name(name)
		{}


		virtual void	Run(Random&random) = 0;

		void			BeginSection(const String&name)	const;
		void			Fail(const TCodeLocation&loc, const String&msg)
		{
			throw Exception(loc,msg);
		}
	};


	typedef std::shared_ptr<Test>	PTest;
	
	extern Ctr::Buffer0<PTest>	all;

	void Run(Test&test);

	class CharacterEncoding : public Test
	{
	public:
		/**/			CharacterEncoding() : Test("Character-Encoding"){}
		virtual void	Run(Random&random) override;
	};

	class ServerClient : public Test
	{
	private:
		TCP::Server server;
		TCP::Client client;
		TCP::Channel<String,0,0>	messageChannel;
		bool			quit = false;

		static ServerClient	*executingInstance;

		static void		OnMessage(String&msg, TCP::Peer&peer);

	public:
		/**/			ServerClient() : Test("Server-Client"),messageChannel(OnMessage)
		{}

		virtual void	Run(Random&random) override;
	};

	class Serialization : public Test
	{
		void			TestRange(serial_size64_t begin, serial_size64_t end, bool expectException);
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

	#ifndef NO_SATELLITES
		class SkyPort : public Test
		{
		public:
			/**/			SkyPort() :Test("SkyPort")
			{}

			virtual void	Run(Random&random)	override;
			static void		EnqueueRandom(Objects::PSkyPort&plant, Random&random);

		};
	#endif

	class Journal : public Test
	{
	public:
		/**/			Journal():Test("Journal"){}
		virtual void	Run(Random&random)	override;
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
	class HighGridFamilies : public Test
	{
	public:
		/**/			HighGridFamilies() : Test("HighGridFamilies")
		{}
		virtual void	Run(Random&random) override;

	};

	class SharedContainer : public Test
	{
	public:
		/**/			SharedContainer() : Test("SharedContainer")
		{}
		virtual void	Run(Random&random) override;
	};

	class TerrainUpdates : public Test
	{
	public:
		/**/			TerrainUpdates() : Test("TerrainUpdates")
		{}
		virtual void	Run(Random&random) override;
	};
}



#endif
