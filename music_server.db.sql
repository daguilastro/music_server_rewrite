BEGIN TRANSACTION;
CREATE TABLE IF NOT EXISTS "artist" (
	"id"	TEXT UNIQUE,
	"name"	TEXT NOT NULL,
	"song count"	INTEGER DEFAULT 0,
	PRIMARY KEY("id")
);
CREATE TABLE IF NOT EXISTS "song" (
	"id"	TEXT,
	"title"	TEXT,
	"artist_id"	TEXT NOT NULL,
	"duration_seconds"	INTEGER NOT NULL,
	"file_path"	TEXT NOT NULL,
	PRIMARY KEY("id"),
	FOREIGN KEY("artist_id") REFERENCES "artist"("id")
);
COMMIT;
