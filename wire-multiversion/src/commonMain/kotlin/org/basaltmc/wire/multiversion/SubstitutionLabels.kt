package org.basaltmc.wire.multiversion

import org.basaltmc.wire.text.Component

/**
 * What to call content a client is too old for and is shown a stand-in of. Wire decides where a name
 * can go; what it says is the server's to write.
 */
public fun interface SubstitutionLabels {
    /** The name for the item [nativeItemId] stands for, or null to leave the substitute unnamed. */
    public fun itemLabel(nativeItemId: Int): Component?

    /** The name to float over an entity of [nativeEntityTypeId], drawn as some other mob. */
    public fun entityLabel(nativeEntityTypeId: Int): Component? = null
}
