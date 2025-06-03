package org.basaltmc.wire.common.placeholder

import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertNull
import kotlin.test.assertTrue

private val NAME = PlaceholderSlot<String>("name")

private object CatalogProvider :
    PlaceholderProvider,
    PlaceholderCatalog {
    override fun names(): Collection<String> = listOf("one", "two")

    override fun resolve(
        params: String,
        context: PlaceholderContext,
    ): PlaceholderValue = PlaceholderValue.of(params)
}

class PlaceholderRegistryTest {
    private fun registry(): PlaceholderRegistry =
        PlaceholderRegistry().apply {
            register("basalt_server") { params, _ ->
                when (params) {
                    "online" -> PlaceholderValue.of(7)
                    "name" -> PlaceholderValue.of("Basalt")
                    "tps" -> PlaceholderValue.Unavailable("nothing measures it yet")
                    else -> PlaceholderValue.Unknown
                }
            }
            register("basalt_player") { params, context ->
                if (params != "name") PlaceholderValue.Unknown else PlaceholderValue.of(context[NAME])
            }
        }

    @Test
    fun theLongestRegisteredIdentifierWins() {
        val registry =
            PlaceholderRegistry().apply {
                register("basalt") { _, _ -> PlaceholderValue.of("short") }
                register("basalt_server") { _, _ -> PlaceholderValue.of("long") }
            }

        // Identifiers contain underscores, so splitting at the first one would send every
        // %basalt_server_*% to whoever registered "basalt".
        assertEquals("long", registry.resolve("basalt_server_online").textOrNull)
        assertEquals("short", registry.resolve("basalt_other").textOrNull)
    }

    @Test
    fun providerForAgreesWithResolveAboutWhoServesAName() {
        val registry = registry()

        // They used to match the identifier separately, and the broken copy sat unnoticed behind the
        // working one: text substituted fine while `/placeholder info` claimed the name was unknown.
        // So: anything that resolves at all must have an identifier to name, and nothing outside
        // every registered identifier may have one.
        for (name in listOf("basalt_server_online", "basalt_server_tps", "basalt_player_name")) {
            assertEquals("basalt_${name.substringAfter('_').substringBefore('_')}", registry.providerFor(name)?.first)
        }
        assertNull(registry.providerFor("someplugin_thing"))
        // A bare identifier has a provider and still no value: it is a family, not a placeholder.
        assertEquals("basalt_server", registry.providerFor("basalt_server")?.first)
        assertEquals(PlaceholderValue.Unknown, registry.resolve("basalt_server"))
    }

    @Test
    fun aCatalogListsItsOwnNamesAndOneThatIsNotACatalogListsNothing() {
        val registry = PlaceholderRegistry()
        registry.register("lambda") { _, _ -> PlaceholderValue.of("x") }
        registry.register("listed", CatalogProvider)

        assertEquals(emptyList(), registry.namesOf("lambda").toList())
        assertEquals(listOf("one", "two"), registry.namesOf("listed").toList())
    }

    @Test
    fun anIdentifierNobodyRegisteredIsUnknownRatherThanEmpty() {
        assertEquals(PlaceholderValue.Unknown, registry().resolve("someplugin_thing"))
    }

    @Test
    fun aKnownNameThisContextCannotAnswerIsToldApartFromAnUnknownOne() {
        val registry = registry()

        // The difference is what lets a warning say "not written yet" instead of "check your
        // spelling", which are different jobs for whoever reads it.
        assertEquals(PlaceholderValue.Unavailable(), registry.resolve("basalt_player_name"))
        assertEquals(PlaceholderValue.Unknown, registry.resolve("basalt_player_nonsense"))
        assertEquals(
            "nothing measures it yet",
            (registry.resolve("basalt_server_tps") as PlaceholderValue.Unavailable).reason,
        )
    }

    @Test
    fun aContextCarriesOnlyWhatWasPutInIt() {
        val registry = registry()
        val context = PlaceholderContext.EMPTY.with(NAME, "Whiron")

        assertEquals("Whiron", registry.resolve("basalt_player_name", context).textOrNull)
        assertNull(registry.resolve("basalt_player_name").textOrNull)
    }

    @Test
    fun registeringTheSameIdentifierTwiceReplacesIt() {
        val registry = registry()
        registry.register("basalt_server") { _, _ -> PlaceholderValue.of("replaced") }

        assertEquals("replaced", registry.resolve("basalt_server_online").textOrNull)
    }
}

class PlaceholderTextTest {
    private val registry =
        PlaceholderRegistry().apply {
            register("basalt_server") { params, _ ->
                if (params == "online") PlaceholderValue.of(7) else PlaceholderValue.Unavailable("not yet")
            }
        }

    @Test
    fun aNameIsReplacedAndOrdinaryProseIsNot() {
        assertEquals("7 online", PlaceholderText.substitute("%basalt_server_online% online", registry))
        // Neither run between two per cent signs is a single word, so this is just a sentence.
        assertEquals("50% off, 20% more", PlaceholderText.substitute("50% off, 20% more", registry))
        assertEquals("100% sure", PlaceholderText.substitute("100%% sure", registry))
    }

    @Test
    fun whatCannotBeAnsweredStaysVisibleAndIsReported() {
        val problems = mutableListOf<String>()
        val result =
            PlaceholderText.substitute("at %basalt_server_tps% now", registry) { name, _ ->
                problems += name
                null
            }

        assertEquals("at %basalt_server_tps% now", result)
        assertEquals(listOf("basalt_server_tps"), problems)
    }

    @Test
    fun aCallerCanChooseWhatGoesInPlaceOfWhatCannotBeAnswered() {
        // The server list does exactly this: a MOTD is read by strangers, and showing them
        // %basalt_player_name% is worse than showing them nothing.
        val blanked = PlaceholderText.substitute("at %basalt_server_tps% now", registry) { _, _ -> "" }

        assertEquals("at  now", blanked)
    }

    @Test
    fun namesInFindsEveryPlaceholderWithoutResolvingAnything() {
        assertEquals(
            listOf("basalt_server_online", "basalt_player_name"),
            PlaceholderText.namesIn("%basalt_server_online% and %basalt_player_name% and 50% off"),
        )
    }

    @Test
    fun globsMatchTheWayTheFilesSayTheyDo() {
        assertTrue(globMatches("*", "anything"))
        assertTrue(globMatches("mc.example.com", "MC.example.com"))
        assertTrue(globMatches("*.example.com", "beta.example.com"))
        assertTrue(globMatches("192.168.*", "192.168.1.4"))
        assertTrue(!globMatches("192.168.*", "10.0.0.1"))
        // "a*bc" must not satisfy itself twice out of the same characters.
        assertTrue(!globMatches("a*bc", "ab"))
        assertTrue(globMatches("a*bc", "abc"))
    }
}

class PlaceholderConditionTest {
    private val registry =
        PlaceholderRegistry().apply {
            register("basalt_player") { params, _ ->
                when (params) {
                    "protocol" -> PlaceholderValue.of(776)
                    "host" -> PlaceholderValue.of("beta.example.com")
                    "name" -> PlaceholderValue.of("Whiron")
                    "known" -> PlaceholderValue.of(true)
                    "x" -> PlaceholderValue.Unavailable("there is no player yet")
                    else -> PlaceholderValue.Unknown
                }
            }
        }

    private fun evaluate(raw: String) = PlaceholderCondition(raw).evaluate(registry)

    @Test
    fun numbersCompareAsNumbersAndTextComparesAsText() {
        assertEquals(ConditionResult.True, evaluate("%basalt_player_protocol% >= 735"))
        assertEquals(ConditionResult.False, evaluate("%basalt_player_protocol% < 735"))
        assertEquals(ConditionResult.True, evaluate("%basalt_player_protocol% = 776"))
        assertEquals(ConditionResult.True, evaluate("%basalt_player_name% = whiron"))
        assertEquals(ConditionResult.True, evaluate("%basalt_player_name% != Someone"))
    }

    @Test
    fun matchesAndContainsWorkOnText() {
        assertEquals(ConditionResult.True, evaluate("%basalt_player_host% matches beta.*"))
        assertEquals(ConditionResult.False, evaluate("%basalt_player_host% matches mc.*"))
        assertEquals(ConditionResult.True, evaluate("%basalt_player_host% contains example"))
    }

    @Test
    fun aConditionWithNoOperatorIsTrueWhenItReadsTrue() {
        assertEquals(ConditionResult.True, evaluate("%basalt_player_known%"))
        assertEquals(ConditionResult.False, evaluate("%basalt_player_name%"))
    }

    @Test
    fun whatCannotBeDecidedIsNotTheSameAsFalse() {
        val unavailable = evaluate("%basalt_player_x% > 100")
        val unknown = evaluate("%basalt_nope% = 1")
        val notANumber = evaluate("%basalt_player_name% > 5")

        assertTrue(unavailable is ConditionResult.Unresolvable && "there is no player yet" in unavailable.reason)
        assertTrue(unknown is ConditionResult.Unresolvable && "not a placeholder" in unknown.reason)
        assertTrue(notANumber is ConditionResult.Unresolvable && "not a number" in notANumber.reason)
    }

    @Test
    fun anOperatorInsideAPlaceholderIsNotAnOperator() {
        // The argument of a placeholder is free text: the separators in a date format must not be
        // read as a comparison.
        val condition = PlaceholderCondition("%basalt_server_time_dd<MM>yyyy% = x")

        assertEquals("%basalt_server_time_dd<MM>yyyy%", condition.raw.substringBefore(" ="))
        assertTrue(condition.evaluate(registry) is ConditionResult.Unresolvable)
    }

    @Test
    fun aWordOperatorHasToBeAWholeWord() {
        val registry = PlaceholderRegistry().apply { register("x") { _, _ -> PlaceholderValue.of("rematches") } }

        // "rematches" contains "matches"; splitting there would compare nonsense to nonsense.
        assertEquals(ConditionResult.True, PlaceholderCondition("%x% = rematches").evaluate(registry))
    }
}
