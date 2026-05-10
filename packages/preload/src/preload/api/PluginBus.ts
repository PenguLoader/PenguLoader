type PluginEventHandler = (payload: unknown, event: PluginBusEvent) => unknown;

type PluginBusEvent = {
  topic: string
  source: string
  timestamp: number
}

type PluginSubscription = {
  owner: string
  handler: PluginEventHandler
  once: boolean
}

const subscriptions = new Map<string, Set<PluginSubscription>>();

function normalizeTopic(topic: string) {
  const normalized = String(topic);

  if (!normalized || normalized.length > 128 || /[\0\r\n]/.test(normalized))
    throw new TypeError('Invalid plugin bus topic');

  return normalized;
}

function reportHandlerError(owner: string, topic: string, error: unknown) {
  console.error('%c Pengu ', 'background: #183461; color: #fff', `Plugin bus handler failed for "${topic}" in "${owner}".`, error);
}

export function createPluginBus(pluginName: string) {
  const bus = {
    on(topic: string, handler: PluginEventHandler) {
      if (typeof handler !== 'function')
        throw new TypeError('Plugin bus handler must be a function');

      const topicName = normalizeTopic(topic);
      const subscription = { owner: pluginName, handler, once: false };
      let topicSubscriptions = subscriptions.get(topicName);

      if (!topicSubscriptions) {
        topicSubscriptions = new Set<PluginSubscription>();
        subscriptions.set(topicName, topicSubscriptions);
      }

      topicSubscriptions.add(subscription);

      let active = true;
      return () => {
        if (!active)
          return;

        active = false;
        const currentSubscriptions = subscriptions.get(topicName);
        if (!currentSubscriptions)
          return;

        currentSubscriptions.delete(subscription);
        if (!currentSubscriptions.size)
          subscriptions.delete(topicName);
      };
    },

    once(topic: string, handler: PluginEventHandler) {
      if (typeof handler !== 'function')
        throw new TypeError('Plugin bus handler must be a function');

      const topicName = normalizeTopic(topic);
      const subscription = { owner: pluginName, handler, once: true };
      let topicSubscriptions = subscriptions.get(topicName);

      if (!topicSubscriptions) {
        topicSubscriptions = new Set<PluginSubscription>();
        subscriptions.set(topicName, topicSubscriptions);
      }

      topicSubscriptions.add(subscription);

      let active = true;
      return () => {
        if (!active)
          return;

        active = false;
        const currentSubscriptions = subscriptions.get(topicName);
        if (!currentSubscriptions)
          return;

        currentSubscriptions.delete(subscription);
        if (!currentSubscriptions.size)
          subscriptions.delete(topicName);
      };
    },

    off(topic: string, handler: PluginEventHandler) {
      const topicName = normalizeTopic(topic);
      const topicSubscriptions = subscriptions.get(topicName);
      if (!topicSubscriptions)
        return;

      for (const subscription of Array.from(topicSubscriptions)) {
        if (subscription.owner === pluginName && subscription.handler === handler)
          topicSubscriptions.delete(subscription);
      }

      if (!topicSubscriptions.size)
        subscriptions.delete(topicName);
    },

    emit(topic: string, payload?: unknown) {
      const topicName = normalizeTopic(topic);
      const topicSubscriptions = Array.from(subscriptions.get(topicName) ?? []);
      if (!topicSubscriptions.length)
        return 0;

      const currentSubscriptions = subscriptions.get(topicName);
      if (currentSubscriptions) {
        for (const subscription of topicSubscriptions) {
          if (subscription.once)
            currentSubscriptions.delete(subscription);
        }

        if (!currentSubscriptions.size)
          subscriptions.delete(topicName);
      }

      const event = Object.freeze({
        topic: topicName,
        source: pluginName,
        timestamp: Date.now(),
      });

      for (const subscription of topicSubscriptions) {
        queueMicrotask(() => {
          try {
            subscription.handler(payload, event);
          } catch (error) {
            reportHandlerError(subscription.owner, topicName, error);
          }
        });
      }

      return topicSubscriptions.length;
    },
  };

  return Object.freeze(bus);
}
