#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")"

# Merge the given yaml files into a single game-ready document:
#   - explode(.) resolves every anchor/alias (the relative links between the
#     files) so the result is easy to read and process.
#   - the lists under each game key are concatenated, so a key such as items:
#     that is spread over several files appears exactly once.
#   - the *-base keys are not copied: their only purpose is to define the
#     anchors used by the relative links, which explode(.) has inlined.
#   - Note: yq's * operator replaces a repeated key and = appends a duplicate
#     one, so += is used to concatenate the lists under a single key.
merge_documents() {
  for file in "$@"; do
    cat "$file"
    printf '\n'
  done \
    | yq ea \
      --yaml-fix-merge-anchor-to-spec=true \
      '
        explode(.)
        | . as $doc ireduce ({};
            .ages          += ($doc.ages // [])
          | .categories    += ($doc.categories // [])
          | .skills        += ($doc.skills // [])
          | .items         += ($doc.items // []))
      '
}

# The tech tree: the ages, the categories, every item definition with its anchors
# and the skills. The game-mode files in game-mode/ select from those anchors.
TECH_TREE=(
  base/ages.yaml
  trade-items/categories.yaml
  trade-items/chemicals.yaml
  trade-items/metals.yaml
  trade-items/minerals.yaml
  trade-items/misc.yaml
  trade-items/seeds.yaml
  skill-tree/skills.yaml
  skill-tree/ages.yaml
  skill-tree/spaceship.yaml
  skill-tree/shipyards.yaml
  skill-tree/lanes.yaml
  skill-tree/survey.yaml
  skill-tree/habitats.yaml
  skill-tree/piracy.yaml
  skill-tree/law.yaml
  skill-tree/trade.yaml
  skill-tree/services.yaml
)

# Every document is built in a temporary directory and moved into place at the end.
# The merge depends on anchors that span the concatenated files, and a yq that scopes
# anchors to one document - v4.54.2 does exactly that - fails part-way through. A
# direct redirect would leave an empty full.yaml behind, newer than its inputs, for
# the next build to read as a complete game.
tmp_dir="$(mktemp -d)"
trap 'rm -rf "$tmp_dir"' EXIT

# Merge the whole tech tree, including every item in the game.
merge_documents "${TECH_TREE[@]}" game-mode/full.yaml > "$tmp_dir/full.yaml"

# Merge the subset of items used by the simple version of the game, for quick
# matches and testing.
merge_documents "${TECH_TREE[@]}" game-mode/simple.yaml > "$tmp_dir/simple.yaml"

# Process the ages into a form useful for the game, exploding all the anchors
# and dropping the base list that only exists to define them.
yq -n \
  --yaml-fix-merge-anchor-to-spec=true \
  'load("base/ages.yaml") | explode(.) | del(.ages-base)' \
  > "$tmp_dir/ages.yaml"

# Process the categories into a form useful for the game, exploding all the
# anchors and dropping the base list that only exists to define them.
yq -n \
  --yaml-fix-merge-anchor-to-spec=true \
  'load("trade-items/categories.yaml") | explode(.) | del(.categories-base)' \
  > "$tmp_dir/categories.yaml"

mv "$tmp_dir/full.yaml" "$tmp_dir/simple.yaml" "$tmp_dir/ages.yaml" "$tmp_dir/categories.yaml" core-config/
