"""SprinKlr-8 zone switches for Home Assistant."""
import requests
import logging
from homeassistant.components.switch import SwitchEntity

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
        [SprinKlr8Zone(host, zone, duration) for zone in range(ZONE_COUNT)],
        update_before_add=True,
    )


class SprinKlr8Zone(SwitchEntity):
    def __init__(self, host, zone, duration):
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

    def turn_on(self, **kwargs):
        try:
            requests.post(
                f"http://{self._host}/api/run_zone",
                json={"zone": self._zone, "duration": self._duration},
                timeout=5,
            )
            self._is_on = True
            self._available = True
        except requests.exceptions.RequestException as e:
            _LOGGER.warning("SprinKlr-8 Zone %d turn_on failed: %s", self._zone + 1, e)
            self._available = False

    def turn_off(self, **kwargs):
        try:
            requests.post(
                f"http://{self._host}/api/stop_sequence",
                timeout=5,
            )
            self._is_on = False
            self._available = True
        except requests.exceptions.RequestException as e:
            _LOGGER.warning("SprinKlr-8 Zone %d turn_off failed: %s", self._zone + 1, e)
            self._available = False

    def update(self):
        try:
            response = requests.get(
                f"http://{self._host}/api/status",
                timeout=5,
            )
            response.raise_for_status()
            data = response.json()
            is_running = data.get("is_running", False)
            active_zone = data.get("active_sprinkler", -1)
            self._is_on = is_running and (active_zone == self._zone)
            self._available = True
        except requests.exceptions.RequestException as e:
            _LOGGER.warning("SprinKlr-8 Zone %d update failed: %s", self._zone + 1, e)
            self._available = False
