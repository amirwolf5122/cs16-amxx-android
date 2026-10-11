package su.xash.cs16client;

/**
 * The CSCZClient launcher icon. Condition Zero content lives in the
 * czero/ game dir, so the only thing this entry changes is which dir
 * the engine gets -- the storage permission flow, the addons install
 * (metamod + AMX Mod X into czero/) and the launch itself are all
 * inherited from MainActivity.
 */
public class CZeroActivity extends MainActivity {
    @Override
    protected String pickGameDir() {
        return "czero";
    }
}
