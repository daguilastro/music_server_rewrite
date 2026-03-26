BEGIN TRANSACTION;
CREATE TABLE IF NOT EXISTS "artist" (
	"id"	TEXT UNIQUE,
	"name"	TEXT NOT NULL,
	"song count"	INTEGER DEFAULT 0,
	PRIMARY KEY("id")
);
CREATE TABLE IF NOT EXISTS "song" (
	"id"	INTEGER,
	"title"	TEXT,
	"channel"	TEXT,
	"duration_seconds"	INTEGER NOT NULL,
	"file_path"	TEXT NOT NULL,
	PRIMARY KEY("id"),
	FOREIGN KEY("channel") REFERENCES "artist"("name")
);
COMMIT;
