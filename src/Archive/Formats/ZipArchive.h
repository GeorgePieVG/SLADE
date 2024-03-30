#pragma once

#include "Archive/Archive.h"

namespace slade
{
class ZipArchive : public Archive
{
public:
	ZipArchive();
	~ZipArchive() override;

	// Opening
	bool open(string_view filename) override; // Open from File
	bool open(const MemChunk& mc) override;   // Open from MemChunk

	// Writing/Saving
	bool write(MemChunk& mc) override;         // Write to MemChunk
	bool write(string_view filename) override; // Write to File

	// Misc
	bool loadEntryData(const ArchiveEntry* entry, MemChunk& out) override;

	// Entry addition/removal
	shared_ptr<ArchiveEntry> addEntry(shared_ptr<ArchiveEntry> entry, string_view add_namespace) override;

	// Detection
	MapDesc         mapDesc(ArchiveEntry* maphead) override;
	vector<MapDesc> detectMaps() override;

	// Search
	ArchiveEntry*         findFirst(ArchiveSearchOptions& options) override;
	ArchiveEntry*         findLast(ArchiveSearchOptions& options) override;
	vector<ArchiveEntry*> findAll(ArchiveSearchOptions& options) override;

	// Static functions
	static bool isZipArchive(const MemChunk& mc);
	static bool isZipArchive(const string& filename);

private:
	string temp_file_;

	void generateTempFileName(string_view filename);
};
} // namespace slade
