clustered-controversy
=====================

Code for computing a "clustered controversy" score, which measures how
concentrated a user is on a focused, controversial topic in a
collective intelligence environment.This documentation is in terms of
users interacting with pages, but "pages" can be another object a user
interacts with (email thread, blog post, etc.). Each of these objects
has a set of features, which are used to determine inter-object
similarity, and a controversy score. Each user has a list of pages
they have interacted with, along with the relative strength of that
interaction (for example, the number of times they have edited a
page).

**make_mmap** _users_file pages_file controversy_file_: Creates memory maps
from text data files. This allows fast querying for the scores of
small numbers of users without loading the (potentially) very large
text files into memory each time. It takes three arguments:

-  users_file: Contains a list of sorted (userid, pageid, edit count)
  tuples, one per line (no parentheses, space separated). Must contain
  every user who will be queried.
-  pages_file: Sorted tuples of the same format as users_file, but with
  the meaning (pageid, featureid, value). Must contain every page
  which is referenced in users_file.
-  controversy_file: Contains a list of tuples of the form (pageid,
  controversy score), one for each page.

Toy examples of each file are provided in the example_data directory.
Outputs users_mmap, pages_mmap, and controversy_mmap, which are simply
binary encodings of the text files for efficient random access. Any of
the arguments can be specified as "_", in which case the associated
output is suppressed.

**cc_mmap** _users_mmap pages_mmap controversy_mmap userids_file threads_:
Takes the memory maps generated above as input, along with a list of
userids in users_file (one per line). Computes the scores in parallel
(using the specified number of threads), writing the group-level
scores to scores_out_X and page-level scores to raw_page_stats_out_X,
where X ranges from 0 to threads - 1. The output formats are:

- scores_out_X: (userid, cc, controversy, clustering) tuples, one per
  line.
- raw_page_stats_out_X: Tuples of the form (userid, page count,
  pageid:controversy/clustering/edit_fraction, ...), one per line.

similarity page_mmap first_pageid second_pageid: Computes the
similarity score between the pages specified.

Example useage
==============
```bash
make
./make_mmap example_data/users example_data/pages example_data/controversy
./cc_mmap users_mmap pages_mmap controversy_mmap example_data/user_list 1
```

Output will be written to scores_out_0 and raw_page_stats_out_0.
