import { defineConfig } from 'astro/config';
import starlight from '@astrojs/starlight';

const structuredData = {
  '@context': 'https://schema.org',
  '@graph': [
    {
      '@type': 'WebSite',
      '@id': 'https://docs.siecs.dev/#website',
      url: 'https://docs.siecs.dev/',
      name: 'SIECS Documentation',
      description: 'Documentation for the SIECS archetype library for C and C++.',
      inLanguage: 'en',
    },
    {
      '@type': 'SoftwareSourceCode',
      '@id': 'https://docs.siecs.dev/#software',
      name: 'SIECS',
      description: 'A compact archetype entity component system for C with a typed C++ API.',
      url: 'https://docs.siecs.dev/',
      codeRepository: 'https://github.com/suleymanlaarabi/siecs',
      programmingLanguage: ['C', 'C++'],
      license: 'https://github.com/suleymanlaarabi/siecs/blob/main/LICENSE',
      image: 'https://docs.siecs.dev/og-image.png',
    },
  ],
};

export default defineConfig({
  site: 'https://docs.siecs.dev',
  base: '/',
  server: {
    port: 5050
  },
  integrations: [
    starlight({
      title: 'SIECS',
      description: 'Technical documentation for the SIECS archetype library for C and C++.',
      favicon: '/favicon.svg',
      customCss: ['./src/styles/custom.css'],
      lastUpdated: true,
      social: [
        {
          icon: 'github',
          label: 'SIECS on GitHub',
          href: 'https://github.com/suleymanlaarabi/siecs',
        },
      ],
      head: [
        {
          tag: 'meta',
          attrs: {
            property: 'og:image',
            content: 'https://docs.siecs.dev/og-image.png',
          },
        },
        {
          tag: 'meta',
          attrs: {
            property: 'og:image:width',
            content: '1200',
          },
        },
        {
          tag: 'meta',
          attrs: {
            property: 'og:image:height',
            content: '630',
          },
        },
        {
          tag: 'meta',
          attrs: {
            property: 'og:image:alt',
            content: 'SIECS archetype documentation for C and C++',
          },
        },
        {
          tag: 'meta',
          attrs: {
            name: 'twitter:image',
            content: 'https://docs.siecs.dev/og-image.png',
          },
        },
        {
          tag: 'script',
          attrs: {
            type: 'application/ld+json',
          },
          content: JSON.stringify(structuredData),
        },
      ],
      sidebar: [
        { label: 'Overview', link: '/' },
        {
          label: 'Getting Started',
          items: ['getting-started', 'cookbook'],
        },
        {
          label: 'Concepts',
          items: [
            'theory',
            'archetype-ecs',
            'entities',
            'components',
            'resources',
            'queries',
            'systems',
            'observers',
            'relations',
            'inheritance',
            'modules',
          ],
        },
        {
          label: 'Guides',
          items: ['ecs-design', 'performance'],
        },
        {
          label: 'Addons',
          items: ['rest'],
        },
        {
          label: 'Reference',
          items: ['reference/api'],
        },
      ],
    }),
  ],
});
