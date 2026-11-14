# Decimal market prices

Manifest uses `Manifest::Core::Decimal` only for per-unit prices: authored base prices,
computed market/local prices, order limits, quote prices and fill unit prices. It stores a
signed 64-bit count of millionths of a credit per item's authored `unitType`.
`0.05` credits is raw `50000`; `5.6312` is raw `5631200`. One whole credit is raw `1000000`.
The largest positive price is `9223372036854.775807` credits per unit.

Quantities, population, production, demand, effect stats, ratios, freight/profit estimates and
physics retain their float/double representations. Existing treasury balances, reserved
credits, settled gross amounts and fees remain whole-credit integers. A decimal price does
not change the unit of a kilogram, litre, ship stat or coordinate. This is the project's
chosen scope; the earlier proposal to convert every economic value to fixed point is superseded.

## Parsing and computation

Authored IDs, YAML keys, unit types and prices stay unchanged. The shipped prices range from
`0.05` to `150000`, and fixtures include `5.6312`; all fit exactly at six decimal places.
Configuration retains the original `basePrice` text. Simulation setup validates it before
replacing an existing world. `Decimal::TryParse` accepts decimal text exactly, rejects
nonfinite spellings, exponents, trailing junk, out-of-range values and excess nonzero decimal
places, and permits extra trailing zeros. Negative prices are rejected by the game boundary.
`Simulation::BasePrice` returns zero for an invalid standalone lookup; setup reports it as an
error naming the item instead of silently treating malformed content as free goods.
Generated economies additionally bound base prices to `Decimal::Max() / 10`, leaving room
for the model's maximum market/local multiplier of 9.5 without overflowing a computed price.

Keep the existing double precision scarcity/local-bias formulas and seeded generation.
Convert each computed price explicitly with `Decimal::TryFromDouble`: round to the nearest
microcredit, halfway away from zero, after checking finiteness and range. This is the only
quantisation of those computed prices. Decimal construction from a floating-point value is
not implicit. Dimensionless ratios and presentation may call `ToDouble` explicitly.

Price ordering, crossing, best-price levels and equality compare raw decimal values exactly.
The quantity epsilon still applies to floating quantities, never to price equality.
An integer midpoint truncates toward zero to the nearest representable microcredit; quantities
and basis-point spread calculations remain doubles. `ToString` formats the stored price
exactly without trailing fractional zeros; display formatting does not alter stored values.

## Settlement and safety

Preserve the existing whole-credit settlement rules: a buy reservation rounds quantity times
limit price up, fill gross rounds quantity times maker price to the nearest whole credit, and
fees round up in whole credits. The unit price remains decimal while quantities remain double;
the mixed product explicitly uses `price.ToDouble() * quantity` to retain existing double
rounding, then checks finite/range conditions before narrowing into the integer ledger.
Changing prices to decimal does not promise microcredit balances or fractional-credit settlement.
The separate development simulation keeps its existing route settlement, truncating buyer
cost and seller revenue to whole credits; it is not the hosted order-book settlement path.

Expected invalid requests return a failure before changing balances/reservations. Decimal
`Try*` arithmetic returns an empty optional on overflow or invalid division. Unchecked arithmetic
is for established invariants and fails through the engine's verification mechanism if those
invariants are violated; it never wraps signed integers or silently saturates a price.

The tests cover exact authored prices, precision/range rejection, explicit computed-price
rounding, exact price levels, reservation/refund behaviour and settlement overflow. Replay
fixtures continue to hash floating quantity state; the priced trade results can change at
microcredit rounding boundaries. Debug, Release and sanitizer checks must agree on both the
state hashes and raw decimal-price hashes. VFS loading, reflected game bindings and
engine-hosted gameplay remain later work.
