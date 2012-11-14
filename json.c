#include <assert.h>
#include <jansson.h>
#include <libspotify/api.h>
#include <string.h>
#include <stdbool.h>

#include "constants.h"

json_t *track_to_json(sp_track *track, json_t *object) {
  char uri[kTrackLinkLength];
  sp_link *link = sp_link_create_from_track(track, 0);
  sp_link_as_string(link, uri, kTrackLinkLength);
  sp_link_release(link);

  json_object_set_new(object, "uri", json_string_nocheck(uri)); 

  if (!sp_track_is_loaded(track))
    return object;

  const char *name = sp_track_name(track);
  json_object_set_new(object, "title", json_string_nocheck(name)); 
  return object;
}

json_t *playlist_to_json_set_collaborative(sp_playlist *playlist,
                                           json_t *object) {
  bool collaborative = sp_playlist_is_collaborative(playlist);
  json_object_set_new(object, "collaborative",
                      collaborative ? json_true() : json_false());
  return object;
}

json_t *playlist_to_json(sp_playlist *playlist, json_t *object) {
  assert(sp_playlist_is_loaded(playlist));

  // Owner
  sp_user *owner = sp_playlist_owner(playlist);
  const char *username = sp_user_display_name(owner);
  sp_user_release(owner);
  json_object_set_new_nocheck(object, "creator",
                              json_string_nocheck(username));

  // URI
  size_t playlist_uri_len = strlen("spotify:user:") + strlen(username) + 
                            strlen(":playlist:") +
                            strlen("284on3DVWeAxWkgVuzZKGt") + 1;
  char *playlist_uri = malloc(playlist_uri_len);

  if (playlist_uri == NULL)
    return NULL;

  sp_link *playlist_link = sp_link_create_from_playlist(playlist);

  if (playlist_link == NULL)  // Shouldn't happen; playlist is loaded (?)
    return object;

  sp_link_as_string(playlist_link, playlist_uri, playlist_uri_len);
  sp_link_release(playlist_link);
  json_object_set_new(object, "uri", 
                      json_string_nocheck(playlist_uri));
  free(playlist_uri);

  // Title
  const char *title = sp_playlist_name(playlist);
  json_object_set_new(object, "title",
                      json_string_nocheck(title));

  // Collaborative
  playlist_to_json_set_collaborative(playlist, object);

  // Description
  const char *description = sp_playlist_get_description(playlist);

  if (description != NULL) {
    json_object_set_new(object, "description",
                        json_string_nocheck(description));
  }

  // Number of subscribers
  int num_subscribers = sp_playlist_num_subscribers(playlist);
  json_object_set_new(object, "subscriberCount",
                      json_integer(num_subscribers));

  // Tracks
  json_t *tracks = json_array();
  json_object_set_new(object, "tracks", tracks);
  char track_uri[kTrackLinkLength];
  char album_uri[kAlbumLinkLength];
  char artist_uri[kArtistLinkLength];

  for (int i = 0; i < sp_playlist_num_tracks(playlist); i++) {
    json_t *track_object = json_object();
    json_t *album_object = json_object();
    json_t *artist_object = json_object();
    
    sp_track *track = sp_playlist_track(playlist, i);
    sp_link *track_link = sp_link_create_from_track(track, 0); // TODO: these can be multiples
    
    sp_album *album = sp_track_album(track);
    sp_link *album_link = sp_link_create_from_album(album);

    sp_artist *artist = sp_track_artist(track, 0); // TODO: these can be multiples
    sp_link *artist_link = sp_link_create_from_artist(artist);

    sp_link_as_string(track_link, track_uri, kTrackLinkLength);
    sp_link_as_string(album_link, album_uri, kTrackLinkLength);
    sp_link_as_string(artist_link, artist_uri, kTrackLinkLength);

    json_object_set_new(track_object, "name", json_string_nocheck(sp_track_name(track)));
    json_object_set_new(track_object, "duration", json_integer(sp_track_duration(track)));
    json_object_set_new(track_object, "uri", json_string_nocheck(track_uri));

    json_object_set_new(album_object, "name", json_string_nocheck(sp_album_name(album)));
    json_object_set_new(album_object, "uri", json_string_nocheck(album_uri));
    json_object_set_new(track_object, "album", album_object);

    json_object_set_new(artist_object, "name", json_string_nocheck(sp_artist_name(artist)));
    json_object_set_new(artist_object, "uri", json_string_nocheck(artist_uri));
    json_object_set_new(track_object, "artist", artist_object);

    json_array_append_new(tracks, track_object);

    sp_link_release(track_link);
    sp_link_release(album_link);
    sp_link_release(artist_link);
  }

  return object;
}

bool json_to_track(json_t *json, sp_track **track) {
  if (!json_is_string(json))
    return false;

  sp_link *link = sp_link_create_from_string(json_string_value(json));

  if (link == NULL)
    return false;

  if (sp_link_type(link) != SP_LINKTYPE_TRACK) {
    sp_link_release(link);
    return false;
  }

  *track = sp_link_as_track(link);
  return *track != NULL;
}

int json_to_tracks(json_t *json, sp_track **tracks, int num_tracks) {
  if (!json_is_array(json))
    return 0;

  int num_valid_tracks = 0;

  for (int i = 0; i < num_tracks; i++) {
    json_t *item = json_array_get(json, i);

    if (json_to_track(item, &tracks[num_valid_tracks]))
      num_valid_tracks++;
  }

  return num_valid_tracks;
}
