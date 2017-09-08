#ifndef fileIOH
#define fileIOH

#include <io/riff_handler.h>
#include <grid/worldData.h>
#include <runtimeConfig.h>


namespace FileIO
{


	namespace Config
	{
		extern int		saveEveryNRounds /*= 10*/;
		extern bool		saveWhenAllQuit /*= true*/;
	}

	extern RuntimeConfig::RoundNumber	lastSaveRound /*= 0*/;
	static const PathString	savFileExtension = "hsf";	//hexaverse save file



	void			Compare(Riff::File&a, Riff::File&b, Ctr::Buffer<Riff::TID>&idStack, const String&indent="");



	void			WriteActualWorldToContainer(Riff::Chunk&container);
	/**
	Serializes @a inData to @a outChunk.
	@param inData Data to serialize. Is not altered. Reference only for compatibilty with existing functions.
	@param outChunk Riff-Chunk to write to
	*/
	void			WriteToListChunk(World::Data&inData, Riff::Chunk&outChunk);
	void			ReadFromListChunk(const Riff::Chunk&, World::Data&);
	void			Save(const PathString&name, RuntimeConfig::Phase savePhase);
	void			Load(const PathString&name);
	bool			CanSaveRightNow();
	bool			IsValidSavePhase(RuntimeConfig::Phase p);

	/**
	Checks whether or not the current configuration and round number makes autosaving appropriate, and executes such.
	@param tempIncrementRoundNumber Increment the current round number before saving, and decrement it again afterwards. Useful when saving just before the round number will be incremented anyways.
	*/
	void			CheckAutoSave(bool tempIncrementRoundNumber);

	/**
	Creates a snapshot of the current world and saves it in the autosave output directory
	*/
	void			AutoSave();


	namespace Network
	{
		/**
		Called automatically by Network::CloseSession()
		Flushes any residual data
		*/
		void		HandleSessionClosure();
	}


	void			RefreshWindowTitle();

	/**
	Retrieves the full path string of the auto-save file with the specified index.
	Note that the respective doesn't necessarily exist.
	*/
	PathString		GetAutoSavePath(index_t index);

}


#endif
