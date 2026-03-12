The alias field in Location is optional, but useful for:

URL rewriting: Serve files from a different directory than the URL suggests.
Mapping virtual paths: Map a URL path to a different filesystem path.
Cases:
Serve static files from a shared directory:

URL: /images/cat.jpg
Alias: /var/www/shared_images
Result: /images/cat.jpg → /var/www/shared_images/cat.jpg
Hide real directory structure:

URL: /docs/manual.pdf
Alias: /srv/manuals
Result: /docs/manual.pdf → /srv/manuals/manual.pdf
Multiple locations share the same alias:

URL: /assets/js/app.js
Alias: /var/www/assets
Result: /assets/js/app.js → /var/www/assets/js/app.js
Summary:
Use alias to map URL paths to different filesystem locations, useful for sharing, hiding, or organizing files.