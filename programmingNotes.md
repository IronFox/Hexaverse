# New type declaration
Declaration a new type is currently spread to several locations, depending on what aspects of the game they affect.
	
## New Structures (buildings) 

### Type Declaration 
First determine the hierarchical location within the existing type tree.
Enter a new declaration in `./shared/objects/types.h`.
You will also need to create a new type constant in 
`./shared/stringTable.h`,  preferably following the `Type_*` naming convention.

### Class Declaration
Place your class declaration in a new .h/.cpp combo.
The class must directly or indirectly inherit `Objects::Structure` and define some classes. To simplify some operations, it is advisable to inherit one of the template base classes `WorkingStructureT<[Type]>` or `OtherStructure<[Type]>`, where `[Type]` is your class name. `WorkingStructureT<T>` inherits from `WorkingStructure` whereas `OtherStructure<T>` does not. Check the documentation of the classes to determine which you need. Also take a look at existing classes (e.g. `Mine`) to figure out what must/should be implemented in your class.

### Class Registration
Commonly, types declare a global prototype instance of themselves allocated as `P[Type] [Type]::globalPrototype`. When using one of the default template base classes (`WorkingStructureT<Type>`, `OtherStructure<Type>`), these will be allocated automatically. Otherwise some other allocation method must be implemented.
Any prototypes allocated this way must be added to the `structures` list via
`Objects::Prototypes::RegisterAll()` in `./shared/objects/prototypes.cpp`.

### Command Declaration
Commands are issued by the client and may or may not have direct impact on the server state. Since many commands affect game aspects other than just objects, they must be declared externally, however stored by your new type.
Currently, commands are declared by  `Objects::Commands::DefineAll()` in `./client/components/visual/commandDefinitions.h`. Your new type must be configured here to have any commands. From here, it may affect any aspect of the client or transmit any data to the server.

### Protocol Expansion
If your new commands cannot be expressed by the existing command channels, then new channels must be allocated.

### Server Command Response
If your command transmitted 