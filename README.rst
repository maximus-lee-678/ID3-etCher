📝 ID3-etCher
==============
| A "C"-imple library that allows for the writing of ID3v2.3 tags to files. Still very much a work-in-progress!
| Follows the ID3v2.3 specification as outlined `here <https://id3.org/id3v2.3.0>`_.

ℹ️ Description
--------------
| ID3-etCher was created to modularise the tagging process original used by MadeInBandcamp, now available for use in any pure C application.
| It allows for the writing of multiple tags to a file, including text tags, comment tags, picture data and more.

* To use in your project, just **#include "include/id3.h"**.
* Finally supports UTF-8 inputs (or UTF-16 to be exact)! Mojibake will be dearly missed.
* Only encodes text as UTF-16 when necessary to save space.

📕 Documentation
-----------------
🚧

🏷️ Tags Supported
------------------
See `supported_tags.rst<https://github.com/maximus-lee-678/ID3-etCher/blob/main/supported_tags.rst>`_ for implementation progress.

🚓 Roadmap
-----------
1. Finish implementing all the tags.
2. Make functions less unwieldy.
3. Implement editing of tags.
4. The Documentation.
