scenario('first', function (c) {
  let loaded;
  c.run(Flow()
    .step(function () {
      const initial = c.test.imageFacts('picture');
      if (!initial.empty || initial.width !== 0 || initial.height !== 0)
        throw Error('picture must start empty');
    }).named('initial-empty')
    .step(function () {
      c.test.deliverChosenFile('chosen', 'Sun.pict');
    }).named('deliver-sun')
    .step(function () {
      loaded = c.test.imageFacts('picture');
      c.test.log('image.load ' + (loaded.empty ? 'fail' : 'ok'));
      c.test.log('image.width ' + loaded.width);
      c.test.log('image.height ' + loaded.height);
      if (loaded.empty || loaded.width <= 0 || loaded.height <= 0)
        throw Error('image did not load after settle');
    }).named('image-loaded')
    .step(function () {
      c.test.deliverChosenFile('chosen', null);
    }).named('deliver-cancel')
    .step(function () {
      const canceled = c.test.imageFacts('picture');
      const unchanged = canceled.empty === loaded.empty &&
        canceled.width === loaded.width && canceled.height === loaded.height;
      c.test.log('cancel.picture.facts ' + (unchanged ? 'unchanged' : 'changed'));
      if (!unchanged) throw Error('cancel changed picture facts');
    }).named('cancel-unchanged'));
});
