"""SprinKlr-8 zone switches for Home Assistant."""
import logging
import aiohttp
from homeassistant.components.switch import SwitchEntity
from homeassistant.helpers.aiohttp_client import async_get_clientsession

_LOGGER = logging.getLogger(__name__)

ZONE_COUNT = 8
DEFAULT_DURATION = 10


async def async_setup_platform(hass, config, async_add_entities, discovery_info=None):
    host = config.get("host")
    duration = config.get("duration", DEFAULT_DURATION)
    if not host:
        _LOGGER.error("SprinKlr-8: 'host' is required in configuration")
        return
    async_add_entities(
        [SprinKlr8Zone(hass, host, zone, duration) for zone in range(ZONE_COUNT)],
        update_before_add=True,
    )


class SprinKlr8Zone(SwitchEntity):
    def __init__(self, hass, host, zone, duration):
        self._hass = hass
        self._host = host
        self._zone = zone
        self._duration = duration
        self._is_on = False
        self._available = True

    @property
    def name(self):
        return f"SprinKlr-8 Zone {self._zone + 1}"

    @property
    def unique_id(self):
        return f"sprinklr8_{self._host}_zone_{self._zone}"

    @property
    def is_on(self):
        return self._is_on

    @property
    def available(self):
        return self._available

    @property
    def device_info(self):
        return {
            "identifiers": {("sprinklr8", self._host)},
            "name": "SprinKlr-8 Controller",
            "manufacturer": "SprinKlr-8",
            "model": "SprinKlr-8 8-Zone WiFi Controller",
        }

    async def async_turn_on(self, **kwargs):
        session = async_get_clientsession(self._hass)
        try:
            async with session.post(
                f"http://{self._host}/api/run_zone",
                json={"zone": self._zone, "duration": self._duration},
                timeout=aiohttp.ClientTimeout(total=5),
            ) as resp:
                if resp.status == 200:
                    self._is_on = True
                self._available = True
        except aiohttp.ClientError as e:
            _LOGGER.warning("SprinKlr-8 Zone %d turn_on failed: %s", self._zone + 1, e)
            self._available = False

    async def async_turn_off(self, **kwargs):
        if not self._is_on:
            return
        session = async_get_clientsession(self._hass)
        try:
            async with session.post(
                f"http://{self._host}/api/stop_sequence",
                timeout=aiohttp.ClientTimeout(total=5),
            ) as resp:
                if resp.status == 200:
                    self._is_on = False
                self._available = True
        except aiohttp.ClientError as e:
            _LOGGER.warning("SprinKlr-8 Zone %d turn_off failed: %s", self._zone + 1, e)
            self._available = False

    async def async_update(self):
        session = async_get_clientsession(self._hass)
        try:
            async with session.get(
                f"http://{self._host}/api/status",
                timeout=aiohttp.ClientTimeout(total=5),
            ) as resp:
                resp.raise_for_status()
                data = await resp.json()
                is_running = data.get("is_running", False)
                active_zone = data.get("active_sprinkler", -1)
                self._is_on = is_running and (active_zone == self._zone)
                self._available = True
        except aiohttp.ClientError as e:
            _LOGGER.warning("SprinKlr-8 Zone %d update failed: %s", self._zone + 1, e)
            self._available = False
