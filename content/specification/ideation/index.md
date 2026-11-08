---
author: Ivan Hawkes
categories:
  - Documentation
  - Concept
  - Idea
date: "2026-11-09"
description: A document that briefly describes the goals and intents of this project.
title: Ideation
type: specification
---

The purpose of this document is to state clearly the idea that will be further
developed into a project. It captures the _why_ and _what_; implementation
detail lives in the requirements, functional, test, and technical
specifications.

## Logline

> I wish to create a simulation of a space trading empire that can be used as the
> basis for a game.

## Raw Ideas

The core of the game is the silk road trade and early shipping trade with China, Japan, the Americas, Africa.

- East India company
- for young men it had a 25% chance of death due to disease, etc but would also set them up for life due to being given crate space for their service - so they too could ship tea and spices.

## Currency

- credits - equivalent to one USD
- store all price information in credits
- for smaller items sell them as a package of 100 or more

## Surveys

- bought with a licence that gives you exclusive access for a 5 year period to a plot of land
- areas with higher likelihood of resource fetch premium prices
- you rent robots and machinery to work your claim on the planet
- you pay haulage to get any rewards to the space station
- warehouses take their fees
- you get what's left
- can be sold at end or extended to 25 / 50 year contracts if profitable
- may also be done on large asteroids with a space station in orbit

## Space Station / Satellite

- think deep space nine / moonbase alpha / the expanse
- trade hubs in orbit around planets
- multiple per planet with their own markets and advantages / disadvantages
- food courts
- groceries, long haul supplies
- bass pro shop for truckers equivalent
- refuel
- dry dock
- repairs
- intel

## Black Market

- trade illegal items
- only found on lawful space stations with few exceptions (largest trade hubs only)
- trades can count towards a fugitive status for you

## Grey Market

- trade items from white market to black market
- can be found on some white lawful space stations and most unlawful space stations
- allows you to trade black market goods without being 'tainted' by the transaction

## Ships

- repair state for important equipment
- are we going to have manned flights?

## Insurance

- research the early Dutch corporation and insurance for long voyages
- costly, but far worse if you get scuppered without any

## Privateer

- research / ideation

## Star Gates / Moon Gates

- extreme range FTL travel is via expensive Star Gate, a ring in close orbit around a sun for gathering extreme amounts of energy and giving them a gravitational boost on top.
- moon gates - in orbit around planets for smaller interstellar trips

## Economy

- GDP is the primary driver of growth
- each economy must be able to be seen through the lens of GDP / capita
- zones with the highest GDP are primarily in banking, information technology, rich in resources, war-like and used the resources of other countries to profit, profited through trade and arbitrage
- the G20 range in GDP from 32 trillion / year down to 500 billion, with per capita PPP (purchasing power parity) around 90,000 at the top and
- the poorest countries have GDP / capita of 230 - 1100, 53 million to 1.8 billion
- that's a factor of 250 PPP and 561,000 total GDP
- we may need a tiered approached for the calculations that take GDP and turn it into points to spend on upgrades. This is because the range of GDP is vast over the course of a world economy and having points scaling from one to several trillion isn't a great design.
- each planet can have multiple working economies which are represented as 'block economies' e.g. US, Europe, Asia,
  Pacific, Russia.
- each economy will operate it's own space station satellite in orbit above it's major land mass
- this provides arbitrage markets which you can trade between on a planetary scale
- corruption is a major influence on GDP and PPP
- investment in health and education pay off in later turns
- education drives research
- each planet has from three to seven trade blocks

### Primary Stats

- corruption index
- GDP 
- PPP
- Natural Resources (renewable and finite) with production capacity and renewal rates
- citizen satisfaction with life level
- health index
- education index
- enegry production
- pollution

## Planetary / Economic Turns

- every 200 ticks
- each tick we calculate the new economy for every planet
- planets have a resource richness chart showing which ones they are rich in
- chart changes over time as the resources are all extracted
- renewable resources can also be ruined by excessive extraction of non-renewable resources changing the climate
- planets can build out resource extraction and transformation based on their populations
- politics and corruption come into play, with each affecting PPP vs GDP
- the planet generates new resources based on extraction capacity
- new transformed goods based on manufacturing and processing capabilities
- we work out the needs of the populace as surplus and deficit
- calculate markets based on supply vs need
- GDP feeds into the game as planet currency, used for planet / corporation / government level unlocks
- corruption can lead to massive wastes of resources in vanity projects e.g. pyramids, super yachts, massive statues of the 'founders'
- happy healthy populace produces more vs one that is sick and tired and ready to rebel
- disasters can occur

## Market Ticks

- every 20 ticks
- settle the books
- opportunity to apply new skills to the market

## Military

- military strength is beyond the domain of this simulation

## Players

- each "player" tick the player's state is calculated

## Player Economy

- start out with your basic adventurers kit e.g. a small ship, some credits, on a crappy planet like Tatooine (outer rim, bad economy). Maybe allow them a 'fortune' skill that can take at game launch to increase their birth fortune. They should be able to get a full cargo bay of grey tier goods.
- trades bring in credit which acts as their form of GDP.

## Spaceship

- every x ticks
- fuel usage
- wear and tear
- random encounter?
- update their position
- if in dock allow trade and re-supply

## Game Mechanics

- user can control the wall clock multiplier e.g. x24 makes 1 day pass for every day of wall clock time
- pause and resume the simulation to give time to think over tech upgrades

## Economy

- use a log (10) function to calculate the fractions of points earned towards a skill point. This will scale all the points allocations down to more sensible values e.g. 1000 credits is 3 points, 10,000,000 is 7 points.
- planet economies tick over in terms of days (need to scale appropriate to journey lengths and wall clock) with one year being 360 of them so they can be reasoned in terms similar to our planet

## Exploitation

- strong economies can force unfair trade and resource exploitation onto weaker economies
- millitary strength is assessed by factoring economic strength and tech tree development

## World Generation

- divide the world up into a 12x6 grid i.e. segments, and then zones within those segments with z1 and z6
 being ice caps (perhaps) and hotter temps around z3 and z4
- not every zone is important enough or viable for exploitation and habitation
- assign climate
- assign resources e.g. gold, coal, oil, copper
- habitability
- now lay out economic blocks in the viable segments - note: uninhabitle might still be exploitable e.g.
  parts of the desert with oil reserves.
- satellite in geo-synchronous orbit above each major block
- world should only span a few ages at most e.g. industrial through to space age, groups outside the main ranges
  for a planet should have minimal effects on the economy

## Essential Skills

- figure out what happens when world lack essential production like medicine, education, communication

## Questions

- who is performing tech upgrades on planets? Is it just an algorithm or can players play the game as a planet admin instead of a space trucker?
- possible ways to play the game
  - turn based, or close to
  - idle game on mobile
  - small Wing Commander style random fights on long hauls
  - interstellar market maker
  - persistant network world that multiple friends can all join, hosted by one player, can be saved by any of them to resume later

## Asteroids

- can be harvested with the right gear
- ice bergs, harvest from the ice belt
  - long haul tossed at planets for very slow movement towards a planet or satellite to aid in terraforming

## Silent Running

- space domes which can provide food, should most likely restrict to truck stops and the like since it doesn't make sense to grow good in space in general.

## Star Wars

- arm the rebellion, arm the empire, arm them both?
- transport insurgents

## Influences

### Cinema

- The Expanse, the politics and hard science reality of life as a multi-planet species
- Andor, politics and struggle of the peasants against the unstoppable power of the ultra-wealthy and the power (political, administrative, financial, news topics, etc) they wield
- Silent Running - the slow death of the last of the natural resources and how almost no-one even cares, an allogory for a lot of what is wrong in this world
- Dune - how a regime can control a single resource and use that to maintain a stranglehold on an entire civilisation

### Games

- Elite (1984) - the blueprint
- Sim City (1989) - when I first realised that games do not need a win condition and resource management can be very engaging
- Wing Commander (1990) - space combat in small chunks with an over-arching story
- World of Warcraft - incremental gains through tech trees, XP, skills, gearing, and knowledge of game mechanics - all with a wonderful community to play and chat with

## Random Thoughts

- should I try and resurrect 'Jettison' and use it for some testing purposes? That would be easier than UE 5.8 since i could easily build and run it locally on Linux and have very fast turn-around times e.g. tech trees might be quick and easy to display using IMGUI, and a basic star system is very easy to render
